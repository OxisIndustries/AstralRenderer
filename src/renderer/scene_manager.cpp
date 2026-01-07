#include "astral/renderer/scene_manager.hpp"
#include "astral/renderer/descriptor_manager.hpp"
#include <spdlog/spdlog.h>
#include <stdexcept>

namespace astral {

SceneManager::SceneManager(Context *context) : m_context(context) {
  auto &descriptorManager = m_context->getDescriptorManager();

  // Resize vectors for double buffering
  m_sceneBuffers.resize(MAX_FRAMES_IN_FLIGHT);
  m_meshInstanceBuffers.resize(MAX_FRAMES_IN_FLIGHT);
  m_indirectBuffers.resize(MAX_FRAMES_IN_FLIGHT);
  m_lightBuffers.resize(MAX_FRAMES_IN_FLIGHT);

  m_sceneBufferIndices.resize(MAX_FRAMES_IN_FLIGHT);
  m_meshInstanceBufferIndices.resize(MAX_FRAMES_IN_FLIGHT);
  m_indirectBufferIndices.resize(MAX_FRAMES_IN_FLIGHT);
  m_lightBufferIndices.resize(MAX_FRAMES_IN_FLIGHT);

  m_meshInstancesPerFrame.resize(MAX_FRAMES_IN_FLIGHT);

  for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
    // Scene Data Buffer
    m_sceneBuffers[i] = std::make_unique<Buffer>(
        m_context, sizeof(SceneData), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
        VMA_MEMORY_USAGE_CPU_TO_GPU);
    m_sceneBufferIndices[i] = descriptorManager.registerBuffer(
        m_sceneBuffers[i]->getHandle(), 0, sizeof(SceneData), 1);

    // Light Buffer
    m_lightBuffers[i] = std::make_unique<Buffer>(
        m_context, sizeof(Light) * MAX_LIGHTS,
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);
    m_lightBufferIndices[i] = descriptorManager.registerBuffer(
        m_lightBuffers[i]->getHandle(), 0, sizeof(Light) * MAX_LIGHTS,
        3); // Binding 3

    // Mesh Instance Buffer
    m_meshInstanceBuffers[i] = std::make_unique<Buffer>(
        m_context, sizeof(MeshInstance) * MAX_MESH_INSTANCES,
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);
    m_meshInstanceBufferIndices[i] = descriptorManager.registerBuffer(
        m_meshInstanceBuffers[i]->getHandle(), 0,
        sizeof(MeshInstance) * MAX_MESH_INSTANCES, 6); // Binding 6

    // Indirect Buffer
    m_indirectBuffers[i] = std::make_unique<Buffer>(
        m_context, sizeof(VkDrawIndexedIndirectCommand) * MAX_MESH_INSTANCES,
        VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT |
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
        VMA_MEMORY_USAGE_CPU_TO_GPU);
    m_indirectBufferIndices[i] = descriptorManager.registerBuffer(
        m_indirectBuffers[i]->getHandle(), 0,
        sizeof(VkDrawIndexedIndirectCommand) * MAX_MESH_INSTANCES,
        7); // Binding 7

    m_meshInstancesPerFrame[i].reserve(MAX_MESH_INSTANCES);
  }

  // Material Metadata Buffer (Static/Shared)
  m_materialBuffer = std::make_unique<Buffer>(
      m_context, sizeof(MaterialMetadata) * MAX_MATERIALS,
      VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);
  m_materialBufferIndex = descriptorManager.registerBuffer(
      m_materialBuffer->getHandle(), 0,
      sizeof(MaterialMetadata) * MAX_MATERIALS, 2); // Binding 2

  // Clustered Rendering Buffers (Static/Shared for now, or managed by compute
  // shader) Note: If cluster culling is dynamic per frame, these might need
  // double buffering too, but typically the grid is static or rebuilt per frame
  // into a GPU-local buffer. The GPU-local buffers (VMA_MEMORY_USAGE_GPU_ONLY)
  // are safe to write from Compute if we use barriers correctly.
  const uint32_t clusterCount = 16 * 9 * 24;
  m_clusterBuffer = std::make_unique<Buffer>(
      m_context, sizeof(Cluster) * clusterCount,
      VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VMA_MEMORY_USAGE_GPU_ONLY);
  m_clusterBufferIndex = descriptorManager.registerBuffer(
      m_clusterBuffer->getHandle(), 0, sizeof(Cluster) * clusterCount,
      8); // Binding 8

  m_lightIndexBuffer = std::make_unique<Buffer>(
      m_context, sizeof(LightIndexList) * clusterCount,
      VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VMA_MEMORY_USAGE_GPU_ONLY);
  m_lightIndexBufferIndex = descriptorManager.registerBuffer(
      m_lightIndexBuffer->getHandle(), 0, sizeof(LightIndexList) * clusterCount,
      9); // Binding 9

  m_materials.reserve(MAX_MATERIALS);
  m_lights.reserve(MAX_LIGHTS);
}

void SceneManager::addMeshInstance(uint32_t frameIndex,
                                   const glm::mat4 &transform,
                                   uint32_t materialIndex, uint32_t indexCount,
                                   uint32_t firstIndex, int vertexOffset,
                                   const glm::vec3 &center, float radius) {
  auto &instances = m_meshInstancesPerFrame[frameIndex];
  if (instances.size() >= MAX_MESH_INSTANCES) {
    spdlog::warn("Maximum mesh instances reached for frame {}!", frameIndex);
    return;
  }

  MeshInstance instance{};
  instance.transform = transform;
  instance.sphereCenter = center;
  instance.sphereRadius = radius;
  instance.materialIndex = materialIndex;
  instances.push_back(instance);

  // Upload instance data to current frame's buffer
  m_meshInstanceBuffers[frameIndex]->upload(&instance, sizeof(MeshInstance),
                                            sizeof(MeshInstance) *
                                                (instances.size() - 1));

  VkDrawIndexedIndirectCommand cmd = {};
  cmd.indexCount = indexCount;
  cmd.instanceCount = 1; // Initially visible
  cmd.firstIndex = firstIndex;
  cmd.vertexOffset = vertexOffset;
  cmd.firstInstance = static_cast<uint32_t>(instances.size() - 1);

  // Upload indirect command to current frame's buffer
  m_indirectBuffers[frameIndex]->upload(
      &cmd, sizeof(VkDrawIndexedIndirectCommand),
      sizeof(VkDrawIndexedIndirectCommand) * (instances.size() - 1));
}

void SceneManager::clearMeshInstances(uint32_t frameIndex) {
  m_meshInstancesPerFrame[frameIndex].clear();
}

void SceneManager::prepareIndirectCommands() {
  // Already handled in addMeshInstance
}

void SceneManager::updateSceneData(uint32_t frameIndex, const SceneData &data) {
  m_sceneBuffers[frameIndex]->upload(&data, sizeof(SceneData));
}

uint32_t SceneManager::addLight(const Light &light) {
  if (m_lights.size() >= MAX_LIGHTS) {
    throw std::runtime_error("Maximum lights reached in SceneManager!");
  }
  uint32_t index = static_cast<uint32_t>(m_lights.size());
  m_lights.push_back(light);
  // Note: We don't upload immediately here, we wait for updateLightsBuffer call
  return index;
}

void SceneManager::updateLight(uint32_t index, const Light &light) {
  if (index >= m_lights.size())
    return;
  m_lights[index] = light;
}

void SceneManager::updateLightsBuffer(uint32_t frameIndex) {
  if (!m_lights.empty()) {
    m_lightBuffers[frameIndex]->upload(m_lights.data(),
                                       sizeof(Light) * m_lights.size(), 0);
  }
}

void SceneManager::removeLight(uint32_t index) {
  if (index >= m_lights.size())
    return;
  m_lights.erase(m_lights.begin() + index);
}

void SceneManager::clearLights() { m_lights.clear(); }

uint32_t SceneManager::addMaterial(const MaterialMetadata &material) {
  if (m_materials.size() >= MAX_MATERIALS) {
    throw std::runtime_error("Maximum materials reached in SceneManager!");
  }

  uint32_t index = static_cast<uint32_t>(m_materials.size());
  m_materials.push_back(material);

  updateMaterial(index, material); // Uses static buffer
  return index;
}

void SceneManager::updateMaterial(uint32_t index,
                                  const MaterialMetadata &material) {
  if (index >= m_materials.size())
    return;

  m_materials[index] = material;
  m_materialBuffer->upload(&material, sizeof(MaterialMetadata),
                           sizeof(MaterialMetadata) * index);
}

} // namespace astral
