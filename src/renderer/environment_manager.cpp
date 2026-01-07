#include "astral/renderer/environment_manager.hpp"
#include "astral/core/commands.hpp"
#include "astral/renderer/compute_pipeline.hpp"
#include "astral/renderer/descriptor_manager.hpp"
#include <filesystem>
#include <fstream>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <spdlog/spdlog.h>
#include <sstream>
#include <stb_image.h>
#include <stdexcept>

namespace astral {

static std::string readFile(const std::string &filename) {
  std::ifstream file(filename, std::ios::ate | std::ios::binary);
  if (!file.is_open()) {
    throw std::runtime_error("Failed to open file: " + filename);
  }
  size_t fileSize = (size_t)file.tellg();
  std::string buffer;
  buffer.resize(fileSize);
  file.seekg(0);
  file.read(buffer.data(), fileSize);
  return buffer;
}

EnvironmentManager::EnvironmentManager(Context *context) : m_context(context) {}

EnvironmentManager::~EnvironmentManager() {
  // Resources are managed by unique_ptrs
}

void EnvironmentManager::loadHDR(const std::string &path) {
  if (!std::filesystem::exists(path)) {
    spdlog::warn("Skybox HDR not found at: {}. IBL will be disabled.", path);
    return;
  }
  spdlog::info("Loading HDR environment map: {}", path);
  int width, height, channels;
  stbi_set_flip_vertically_on_load(true);
  float *data = stbi_loadf(path.c_str(), &width, &height, &channels, 4);
  stbi_set_flip_vertically_on_load(false);

  if (!data) {
    spdlog::error("Failed to load HDR image: {}", path);
    return;
  }

  ImageSpecs equirectSpecs;
  equirectSpecs.width = static_cast<uint32_t>(width);
  equirectSpecs.height = static_cast<uint32_t>(height);
  equirectSpecs.format = VK_FORMAT_R32G32B32A32_SFLOAT;
  equirectSpecs.usage =
      VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;

  auto equirect = std::make_unique<Image>(m_context, equirectSpecs);
  equirect->upload(data, width * height * 4 * sizeof(float));
  stbi_image_free(data);

  // Convert to Cubemap
  convertEquirectToCube(equirect);

  // Generate IBL maps
  generateIrradiance();
  generatePrefiltered();
  generateBrdfLut();

  spdlog::info("Environment IBL maps generated successfully.");
}

void EnvironmentManager::convertEquirectToCube(
    const std::unique_ptr<Image> &equirect) {
  uint32_t cubemapSize = 1024;

  ImageSpecs cubeSpecs;
  cubeSpecs.width = cubemapSize;
  cubeSpecs.height = cubemapSize;
  cubeSpecs.format = VK_FORMAT_R16G16B16A16_SFLOAT;
  cubeSpecs.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT |
                    VK_IMAGE_USAGE_TRANSFER_DST_BIT;
  cubeSpecs.arrayLayers = 6;
  cubeSpecs.viewType = VK_IMAGE_VIEW_TYPE_CUBE;

  m_skybox = std::make_unique<Image>(m_context, cubeSpecs);

  VkSamplerCreateInfo samplerInfo = {VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};
  samplerInfo.magFilter = VK_FILTER_LINEAR;
  samplerInfo.minFilter = VK_FILTER_LINEAR;
  samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
  samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
  samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
  samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;

  VkSampler sampler;
  vkCreateSampler(m_context->getDevice(), &samplerInfo, nullptr, &sampler);

  uint32_t equirectIdx = m_context->getDescriptorManager().registerImage(
      equirect->getView(), sampler);
  uint32_t cubeIdx = m_context->getDescriptorManager().registerStorageImage(
      m_skybox->getView());
  m_skyboxIndex = m_context->getDescriptorManager().registerImageCube(
      m_skybox->getView(), sampler);

  auto compShader = std::make_shared<Shader>(
      m_context, readFile("assets/shaders/equirect_to_cube.comp.spv"),
      ShaderStage::Compute, "EquirectToCube");

  VkPushConstantRange pushRange = {};
  pushRange.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
  pushRange.offset = 0;
  pushRange.size = sizeof(uint32_t) * 2;

  VkPipelineLayoutCreateInfo layoutInfo = {
      VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
  layoutInfo.pushConstantRangeCount = 1;
  layoutInfo.pPushConstantRanges = &pushRange;
  VkDescriptorSetLayout setLayouts[] = {
      m_context->getDescriptorManager().getLayout()};
  layoutInfo.setLayoutCount = 1;
  layoutInfo.pSetLayouts = setLayouts;

  VkPipelineLayout layout;
  vkCreatePipelineLayout(m_context->getDevice(), &layoutInfo, nullptr, &layout);

  ComputePipelineSpecs specs;
  specs.computeShader = compShader;
  specs.layout = layout;
  ComputePipeline pipeline(m_context, specs);

  ImmediateCommands cmd(m_context);
  VkCommandBuffer cb = cmd.getBuffer();

  // Transition skybox to GENERAL for storage writing
  {
    VkImageMemoryBarrier barrier = {VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
    barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    barrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
    barrier.srcAccessMask = 0;
    barrier.dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
    barrier.image = m_skybox->getHandle();
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.levelCount = 1;
    barrier.subresourceRange.layerCount = 6;
    vkCmdPipelineBarrier(cb, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                         VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, nullptr, 0,
                         nullptr, 1, &barrier);
  }

  vkCmdBindPipeline(cb, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline.getHandle());
  VkDescriptorSet set = m_context->getDescriptorManager().getDescriptorSet();
  vkCmdBindDescriptorSets(cb, VK_PIPELINE_BIND_POINT_COMPUTE, layout, 0, 1,
                          &set, 0, nullptr);

  uint32_t pcs[] = {equirectIdx, cubeIdx};
  vkCmdPushConstants(cb, layout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(pcs),
                     pcs);

  vkCmdDispatch(cb, cubemapSize / 16, cubemapSize / 16, 6);

  // Barrier for skybox generation
  VkImageMemoryBarrier barrier = {VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
  barrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
  barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
  barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
  barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
  barrier.image = m_skybox->getHandle();
  barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  barrier.subresourceRange.levelCount = 1;
  barrier.subresourceRange.layerCount = 6;
  vkCmdPipelineBarrier(cb, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                       VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, nullptr, 0,
                       nullptr, 1, &barrier);

  vkDestroyPipelineLayout(m_context->getDevice(), layout, nullptr);
}

void EnvironmentManager::generateIrradiance() {
  uint32_t irradianceSize = 32;

  ImageSpecs irrSpecs;
  irrSpecs.width = irradianceSize;
  irrSpecs.height = irradianceSize;
  irrSpecs.format = VK_FORMAT_R16G16B16A16_SFLOAT;
  irrSpecs.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT;
  irrSpecs.arrayLayers = 6;
  irrSpecs.viewType = VK_IMAGE_VIEW_TYPE_CUBE;

  m_irradiance = std::make_unique<Image>(m_context, irrSpecs);

  VkSamplerCreateInfo samplerInfo = {VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};
  samplerInfo.magFilter = VK_FILTER_LINEAR;
  samplerInfo.minFilter = VK_FILTER_LINEAR;
  samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
  samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
  samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;

  VkSampler sampler;
  vkCreateSampler(m_context->getDevice(), &samplerInfo, nullptr, &sampler);

  uint32_t outputIdx = m_context->getDescriptorManager().registerStorageImage(
      m_irradiance->getView());
  m_irradianceIndex = m_context->getDescriptorManager().registerImageCube(
      m_irradiance->getView(), sampler);

  auto compShader = std::make_shared<Shader>(
      m_context, readFile("assets/shaders/irradiance.comp.spv"),
      ShaderStage::Compute, "IrradianceMap");

  VkPushConstantRange pushRange = {};
  pushRange.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
  pushRange.offset = 0;
  pushRange.size = sizeof(uint32_t) * 2;

  VkPipelineLayoutCreateInfo layoutInfo = {
      VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
  layoutInfo.pushConstantRangeCount = 1;
  layoutInfo.pPushConstantRanges = &pushRange;
  VkDescriptorSetLayout setLayouts[] = {
      m_context->getDescriptorManager().getLayout()};
  layoutInfo.setLayoutCount = 1;
  layoutInfo.pSetLayouts = setLayouts;

  VkPipelineLayout layout;
  vkCreatePipelineLayout(m_context->getDevice(), &layoutInfo, nullptr, &layout);

  ComputePipelineSpecs specs;
  specs.computeShader = compShader;
  specs.layout = layout;
  ComputePipeline pipeline(m_context, specs);

  ImmediateCommands cmd(m_context);
  VkCommandBuffer cb = cmd.getBuffer();

  // Transition irradiance to GENERAL for storage writing
  {
    VkImageMemoryBarrier barrier = {VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
    barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    barrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
    barrier.srcAccessMask = 0;
    barrier.dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
    barrier.image = m_irradiance->getHandle();
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.levelCount = 1;
    barrier.subresourceRange.layerCount = 6;
    vkCmdPipelineBarrier(cb, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                         VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, nullptr, 0,
                         nullptr, 1, &barrier);
  }

  vkCmdBindPipeline(cb, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline.getHandle());
  VkDescriptorSet set = m_context->getDescriptorManager().getDescriptorSet();
  vkCmdBindDescriptorSets(cb, VK_PIPELINE_BIND_POINT_COMPUTE, layout, 0, 1,
                          &set, 0, nullptr);

  uint32_t pcs[] = {m_skyboxIndex, outputIdx};
  vkCmdPushConstants(cb, layout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(pcs),
                     pcs);

  vkCmdDispatch(cb, irradianceSize / 16, irradianceSize / 16, 6);

  // Barrier for irradiance generation
  VkImageMemoryBarrier barrier = {VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
  barrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
  barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
  barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
  barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
  barrier.image = m_irradiance->getHandle();
  barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  barrier.subresourceRange.levelCount = 1;
  barrier.subresourceRange.layerCount = 6;
  vkCmdPipelineBarrier(cb, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                       VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, nullptr, 0,
                       nullptr, 1, &barrier);

  vkDestroyPipelineLayout(m_context->getDevice(), layout, nullptr);
}

void EnvironmentManager::generatePrefiltered() {
  uint32_t prefilteredSize = 512;
  uint32_t mipLevels = 5;

  ImageSpecs prefSpecs;
  prefSpecs.width = prefilteredSize;
  prefSpecs.height = prefilteredSize;
  prefSpecs.format = VK_FORMAT_R16G16B16A16_SFLOAT;
  prefSpecs.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT;
  prefSpecs.arrayLayers = 6;
  prefSpecs.mipLevels = mipLevels;
  prefSpecs.viewType = VK_IMAGE_VIEW_TYPE_CUBE;

  m_prefiltered = std::make_unique<Image>(m_context, prefSpecs);

  VkSamplerCreateInfo samplerInfo = {VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};
  samplerInfo.magFilter = VK_FILTER_LINEAR;
  samplerInfo.minFilter = VK_FILTER_LINEAR;
  samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
  samplerInfo.minLod = 0.0f;
  samplerInfo.maxLod = static_cast<float>(mipLevels);
  samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
  samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
  samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;

  VkSampler sampler;
  vkCreateSampler(m_context->getDevice(), &samplerInfo, nullptr, &sampler);

  m_prefilteredIndex = m_context->getDescriptorManager().registerImageCube(
      m_prefiltered->getView(), sampler);

  auto compShader = std::make_shared<Shader>(
      m_context, readFile("assets/shaders/prefilter.comp.spv"),
      ShaderStage::Compute, "PrefilterMap");

  VkPushConstantRange pushRange = {};
  pushRange.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
  pushRange.offset = 0;
  pushRange.size = sizeof(uint32_t) * 2 + sizeof(float);

  VkPipelineLayoutCreateInfo layoutInfo = {
      VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
  layoutInfo.pushConstantRangeCount = 1;
  layoutInfo.pPushConstantRanges = &pushRange;
  VkDescriptorSetLayout setLayouts[] = {
      m_context->getDescriptorManager().getLayout()};
  layoutInfo.setLayoutCount = 1;
  layoutInfo.pSetLayouts = setLayouts;

  VkPipelineLayout layout;
  vkCreatePipelineLayout(m_context->getDevice(), &layoutInfo, nullptr, &layout);

  ComputePipelineSpecs specs;
  specs.computeShader = compShader;
  specs.layout = layout;
  ComputePipeline pipeline(m_context, specs);

  ImmediateCommands cmd(m_context);
  VkCommandBuffer cb = cmd.getBuffer();

  // Transition prefiltered to GENERAL for storage writing
  {
    VkImageMemoryBarrier barrier = {VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
    barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    barrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
    barrier.srcAccessMask = 0;
    barrier.dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
    barrier.image = m_prefiltered->getHandle();
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.levelCount = mipLevels;
    barrier.subresourceRange.layerCount = 6;
    vkCmdPipelineBarrier(cb, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                         VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, nullptr, 0,
                         nullptr, 1, &barrier);
  }

  for (uint32_t i = 0; i < mipLevels; ++i) {
    uint32_t mipWidth = prefilteredSize >> i;
    uint32_t mipHeight = prefilteredSize >> i;
    float roughness = static_cast<float>(i) / static_cast<float>(mipLevels - 1);

    // We need a view for each mip level for storage image access
    VkImageViewCreateInfo viewInfo = {VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
    viewInfo.image = m_prefiltered->getHandle();
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_CUBE;
    viewInfo.format = VK_FORMAT_R16G16B16A16_SFLOAT;
    viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    viewInfo.subresourceRange.baseMipLevel = i;
    viewInfo.subresourceRange.levelCount = 1;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount = 6;

    VkImageView mipView;
    vkCreateImageView(m_context->getDevice(), &viewInfo, nullptr, &mipView);
    uint32_t mipOutputIdx =
        m_context->getDescriptorManager().registerStorageImage(mipView);

    vkCmdBindPipeline(cb, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline.getHandle());
    VkDescriptorSet set = m_context->getDescriptorManager().getDescriptorSet();
    vkCmdBindDescriptorSets(cb, VK_PIPELINE_BIND_POINT_COMPUTE, layout, 0, 1,
                            &set, 0, nullptr);

    struct PushConstants {
      uint32_t inputIdx;
      uint32_t outputIdx;
      float roughness;
    } pc;
    pc.inputIdx = m_skyboxIndex;
    pc.outputIdx = mipOutputIdx;
    pc.roughness = roughness;
    vkCmdPushConstants(cb, layout, VK_SHADER_STAGE_COMPUTE_BIT, 0,
                       sizeof(PushConstants), &pc);

    vkCmdDispatch(cb, std::max(1u, mipWidth / 16), std::max(1u, mipHeight / 16),
                  6);

    // Note: In a production renderer, we would cleanup these views or reuse
    // them. For now we just let them leak as this is a one-time startup
    // operation.
  }

  // Barrier for prefiltered generation
  VkImageMemoryBarrier barrier = {VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
  barrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
  barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
  barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
  barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
  barrier.image = m_prefiltered->getHandle();
  barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  barrier.subresourceRange.levelCount = mipLevels;
  barrier.subresourceRange.layerCount = 6;
  vkCmdPipelineBarrier(cb, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                       VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, nullptr, 0,
                       nullptr, 1, &barrier);

  vkDestroyPipelineLayout(m_context->getDevice(), layout, nullptr);
}

void EnvironmentManager::generateBrdfLut() {
  uint32_t lutSize = 512;

  ImageSpecs lutSpecs;
  lutSpecs.width = lutSize;
  lutSpecs.height = lutSize;
  lutSpecs.format = VK_FORMAT_R16G16_SFLOAT;
  lutSpecs.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT;

  m_brdfLut = std::make_unique<Image>(m_context, lutSpecs);

  VkSamplerCreateInfo samplerInfo = {VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};
  samplerInfo.magFilter = VK_FILTER_LINEAR;
  samplerInfo.minFilter = VK_FILTER_LINEAR;
  samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
  samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
  samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;

  VkSampler sampler;
  vkCreateSampler(m_context->getDevice(), &samplerInfo, nullptr, &sampler);

  uint32_t outputIdx = m_context->getDescriptorManager().registerStorageImage(
      m_brdfLut->getView());
  m_brdfLutIndex = m_context->getDescriptorManager().registerImage(
      m_brdfLut->getView(), sampler);

  auto compShader = std::make_shared<Shader>(
      m_context, readFile("assets/shaders/brdf_lut.comp.spv"),
      ShaderStage::Compute, "BrdfLut");

  VkPushConstantRange pushRange = {};
  pushRange.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
  pushRange.offset = 0;
  pushRange.size = sizeof(uint32_t);

  VkPipelineLayoutCreateInfo layoutInfo = {
      VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
  layoutInfo.pushConstantRangeCount = 1;
  layoutInfo.pPushConstantRanges = &pushRange;
  VkDescriptorSetLayout setLayouts[] = {
      m_context->getDescriptorManager().getLayout()};
  layoutInfo.setLayoutCount = 1;
  layoutInfo.pSetLayouts = setLayouts;

  VkPipelineLayout layout;
  vkCreatePipelineLayout(m_context->getDevice(), &layoutInfo, nullptr, &layout);

  ComputePipelineSpecs specs;
  specs.computeShader = compShader;
  specs.layout = layout;
  ComputePipeline pipeline(m_context, specs);

  ImmediateCommands cmd(m_context);
  VkCommandBuffer cb = cmd.getBuffer();

  // Transition BRDF LUT to GENERAL for storage writing
  {
    VkImageMemoryBarrier barrier = {VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
    barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    barrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
    barrier.srcAccessMask = 0;
    barrier.dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
    barrier.image = m_brdfLut->getHandle();
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.levelCount = 1;
    barrier.subresourceRange.layerCount = 1;
    vkCmdPipelineBarrier(cb, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                         VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, nullptr, 0,
                         nullptr, 1, &barrier);
  }

  vkCmdBindPipeline(cb, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline.getHandle());
  VkDescriptorSet set = m_context->getDescriptorManager().getDescriptorSet();
  vkCmdBindDescriptorSets(cb, VK_PIPELINE_BIND_POINT_COMPUTE, layout, 0, 1,
                          &set, 0, nullptr);

  vkCmdPushConstants(cb, layout, VK_SHADER_STAGE_COMPUTE_BIT, 0,
                     sizeof(uint32_t), &outputIdx);

  vkCmdDispatch(cb, lutSize / 16, lutSize / 16, 1);

  // Barrier for BRDF LUT generation
  VkImageMemoryBarrier barrier = {VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
  barrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
  barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
  barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
  barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
  barrier.image = m_brdfLut->getHandle();
  barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  barrier.subresourceRange.levelCount = 1;
  barrier.subresourceRange.layerCount = 1;
  vkCmdPipelineBarrier(cb, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                       VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, nullptr, 0,
                       nullptr, 1, &barrier);

  vkDestroyPipelineLayout(m_context->getDevice(), layout, nullptr);
}

} // namespace astral
