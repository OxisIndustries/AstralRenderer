#include "astral/renderer/scene_manager.hpp"
#include "astral/renderer/descriptor_manager.hpp"
#include <spdlog/spdlog.h>
#include <stdexcept>
#include <algorithm>

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

  m_frameInstances.resize(MAX_FRAMES_IN_FLIGHT);

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

    m_frameInstances[i].reserve(MAX_MESH_INSTANCES);
  }

  // Material Buffer (Static/Shared/Bindless-indexed)
  // Using binding 2 for now, should match shader logic.
  m_materialBuffer = std::make_unique<Buffer>(
      m_context, sizeof(MaterialGPU) * MAX_MATERIALS,
      VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);

  m_materialBufferIndex = descriptorManager.registerBuffer(
       m_materialBuffer->getHandle(), 0,
       sizeof(MaterialGPU) * MAX_MATERIALS, 2); 
  // m_materialBufferIndex = 0; // Dummy value
       
  m_materials.reserve(MAX_MATERIALS);
  m_gpuMaterials.reserve(MAX_MATERIALS);
  m_lights.reserve(MAX_LIGHTS);
}

void SceneManager::addMeshInstance(uint32_t frameIndex,
                                   const glm::mat4 &transform,
                                   uint32_t materialIndex, uint32_t indexCount,
                                   uint32_t firstIndex, int vertexOffset,
                                   const glm::vec3 &center, float radius) {
  auto &instances = m_frameInstances[frameIndex];
  if (instances.size() >= MAX_MESH_INSTANCES) {
    spdlog::warn("Maximum mesh instances reached for frame {}!", frameIndex);
    return;
  }

  MeshInstance instance{};
  instance.transform = transform;
  instance.sphereCenter = center;
  instance.sphereRadius = radius;
  instance.materialIndex = materialIndex;
  
  FrameMeshInstance fmi;
  fmi.meshInstance = instance;
  fmi.indexCount = indexCount;
  fmi.firstIndex = firstIndex;
  fmi.vertexOffset = vertexOffset;

  instances.push_back(fmi);
}

void SceneManager::clearMeshInstances(uint32_t frameIndex) {
  m_frameInstances[frameIndex].clear();
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


void SceneManager::addModel(std::unique_ptr<Model> model) {
    m_models.push_back(std::move(model));
}

int32_t SceneManager::addMaterial(const Material& material) {
    if (m_materials.size() >= MAX_MATERIALS) {
        spdlog::warn("Maximum materials reached!");
        return -1;
    }
    
    int32_t index = static_cast<int32_t>(m_materials.size());
    m_materials.push_back(material);
    m_gpuMaterials.push_back(material.gpuData);
    
    m_materialsDirty = true;
    return index;
}

void SceneManager::updateMaterialBuffer() {
  if (m_materialsDirty) {
    if (m_gpuMaterials.size() > 0) {
       m_materialBuffer->upload(m_gpuMaterials.data(), m_gpuMaterials.size() * sizeof(MaterialGPU));
    }
    m_materialsDirty = false;
  }
}

void SceneManager::updateMaterial(uint32_t index, const Material& material) {
    if (index < m_materials.size()) {
        m_materials[index] = material;
        // Update GPU copy
        m_gpuMaterials[index] = material.gpuData;
        
        m_materialsDirty = true;
    }
}

void SceneManager::sortAndUploadInstances(uint32_t frameIndex, const glm::vec3& cameraPos) {
    auto& instances = m_frameInstances[frameIndex];
    if (instances.empty()) return;

    // 1. Separate Opaque and Transparent
    std::vector<FrameMeshInstance> opaque;
    std::vector<FrameMeshInstance> transparent;
    opaque.reserve(instances.size());
    transparent.reserve(instances.size()); 

    for (const auto& inst : instances) {
        if (inst.meshInstance.materialIndex < m_gpuMaterials.size()) {
             if (m_gpuMaterials[inst.meshInstance.materialIndex].alphaMode == static_cast<uint32_t>(AlphaMode::Blend)) {
                 transparent.push_back(inst);
             } else {
                 opaque.push_back(inst);
             }
        } else {
            opaque.push_back(inst);
        }
    }

    // 2. Sort Opaque (Front to Back)
    std::sort(opaque.begin(), opaque.end(), [&](const FrameMeshInstance& a, const FrameMeshInstance& b) {
        float distA = glm::distance(a.meshInstance.sphereCenter, cameraPos);
        float distB = glm::distance(b.meshInstance.sphereCenter, cameraPos);
        return distA < distB;
    });

    // 3. Sort Transparent (Back to Front)
    std::sort(transparent.begin(), transparent.end(), [&](const FrameMeshInstance& a, const FrameMeshInstance& b) {
        float distA = glm::distance(a.meshInstance.sphereCenter, cameraPos);
        float distB = glm::distance(b.meshInstance.sphereCenter, cameraPos);
        return distA > distB;
    });

    // 4. Merge back to instances
    instances.clear();
    instances.insert(instances.end(), opaque.begin(), opaque.end());
    instances.insert(instances.end(), transparent.begin(), transparent.end());
    
    // Store Opaque Count
    if (m_opaqueInstanceCounts.size() < MAX_FRAMES_IN_FLIGHT) {
        m_opaqueInstanceCounts.resize(MAX_FRAMES_IN_FLIGHT);
    }
    m_opaqueInstanceCounts[frameIndex] = static_cast<uint32_t>(opaque.size());

    // 5. Upload to GPU
    std::vector<MeshInstance> gpuInstances;
    std::vector<VkDrawIndexedIndirectCommand> commands;
    gpuInstances.reserve(instances.size());
    commands.reserve(instances.size());

    for (size_t i = 0; i < instances.size(); ++i) {
        gpuInstances.push_back(instances[i].meshInstance);
        
        VkDrawIndexedIndirectCommand cmd{};
        cmd.indexCount = instances[i].indexCount;
        cmd.instanceCount = 1;
        cmd.firstIndex = instances[i].firstIndex;
        cmd.vertexOffset = instances[i].vertexOffset;
        cmd.firstInstance = static_cast<uint32_t>(i);
        commands.push_back(cmd);
    }

    m_meshInstanceBuffers[frameIndex]->upload(gpuInstances.data(), gpuInstances.size() * sizeof(MeshInstance));
    m_indirectBuffers[frameIndex]->upload(commands.data(), commands.size() * sizeof(VkDrawIndexedIndirectCommand));
}

} // namespace astral
