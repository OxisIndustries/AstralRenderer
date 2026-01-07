#pragma once

#include "astral/core/context.hpp"
#include "astral/renderer/scene_data.hpp"
#include "astral/resources/buffer.hpp"
#include <memory>
#include <vector>

namespace astral {

struct MeshInstance {
  glm::mat4 transform;
  glm::vec3 sphereCenter;
  float sphereRadius;
  uint32_t materialIndex;
  uint32_t padding[3];
};

struct Cluster {
  glm::vec4 minPoint;
  glm::vec4 maxPoint;
};

struct LightIndexList {
  uint32_t count;
  uint32_t indices[255];
};

class SceneManager {
public:
  SceneManager(Context *context);
  ~SceneManager() = default;

  void updateSceneData(uint32_t frameIndex, const SceneData &data);

  // Light management
  uint32_t addLight(const Light &light);
  void updateLight(uint32_t index, const Light &light);
  void updateLightsBuffer(uint32_t frameIndex);
  void removeLight(uint32_t index);
  void clearLights();
  const std::vector<Light> &getLights() const { return m_lights; }
  uint32_t getLightBufferIndex(uint32_t frameIndex) const {
    return m_lightBufferIndices[frameIndex];
  }

  // Material management
  uint32_t addMaterial(const MaterialMetadata &material);
  void updateMaterial(uint32_t index, const MaterialMetadata &material);

  const std::vector<MaterialMetadata> &getMaterials() const {
    return m_materials;
  }
  size_t getMaterialCount() const { return m_materials.size(); }
  const MaterialMetadata &getMaterial(uint32_t index) const {
    return m_materials[index];
  }

  VkBuffer getSceneBuffer(uint32_t frameIndex) const {
    return m_sceneBuffers[frameIndex]->getHandle();
  }
  VkBuffer getMaterialBuffer() const { return m_materialBuffer->getHandle(); }

  uint32_t getSceneBufferIndex(uint32_t frameIndex) const {
    return m_sceneBufferIndices[frameIndex];
  }
  uint32_t getMaterialBufferIndex() const { return m_materialBufferIndex; }
  uint32_t getMeshInstanceBufferIndex(uint32_t frameIndex) const {
    return m_meshInstanceBufferIndices[frameIndex];
  }
  uint32_t getIndirectBufferIndex(uint32_t frameIndex) const {
    return m_indirectBufferIndices[frameIndex];
  }
  uint32_t getClusterBufferIndex() const { return m_clusterBufferIndex; }
  uint32_t getLightIndexBufferIndex() const { return m_lightIndexBufferIndex; }

  void addMeshInstance(uint32_t frameIndex, const glm::mat4 &transform,
                       uint32_t materialIndex, uint32_t indexCount,
                       uint32_t firstIndex, int vertexOffset,
                       const glm::vec3 &center, float radius);
  void prepareIndirectCommands();
  void clearMeshInstances(uint32_t frameIndex);

  size_t getMeshInstanceCount(uint32_t frameIndex) const {
    return m_meshInstancesPerFrame[frameIndex].size();
  }

  VkBuffer getMeshInstanceBuffer(uint32_t frameIndex) const {
    return m_meshInstanceBuffers[frameIndex]->getHandle();
  }
  VkBuffer getIndirectBuffer(uint32_t frameIndex) const {
    return m_indirectBuffers[frameIndex]->getHandle();
  }
  VkBuffer getClusterBuffer() const { return m_clusterBuffer->getHandle(); }
  VkBuffer getLightIndexBuffer() const {
    return m_lightIndexBuffer->getHandle();
  }

private:
  Context *m_context;
  static const int MAX_FRAMES_IN_FLIGHT = 2;

  std::vector<std::unique_ptr<Buffer>> m_sceneBuffers;
  std::vector<std::unique_ptr<Buffer>> m_meshInstanceBuffers;
  std::vector<std::unique_ptr<Buffer>> m_indirectBuffers;
  std::vector<std::unique_ptr<Buffer>> m_lightBuffers;

  // Static buffers (update rarely or handled differently)
  std::unique_ptr<Buffer> m_materialBuffer;
  std::unique_ptr<Buffer> m_clusterBuffer;
  std::unique_ptr<Buffer> m_lightIndexBuffer;

  std::vector<uint32_t> m_sceneBufferIndices;
  std::vector<uint32_t> m_meshInstanceBufferIndices;
  std::vector<uint32_t> m_indirectBufferIndices;
  std::vector<uint32_t> m_lightBufferIndices;

  uint32_t m_materialBufferIndex;
  uint32_t m_clusterBufferIndex;
  uint32_t m_lightIndexBufferIndex;

  std::vector<MaterialMetadata> m_materials;
  std::vector<Light> m_lights;

  // Per frame instance data
  std::vector<std::vector<MeshInstance>>
      m_meshInstancesPerFrame; // [frame][instance]

  static constexpr uint32_t MAX_MATERIALS = 1000;
  static constexpr uint32_t MAX_LIGHTS = 256;
  static constexpr uint32_t MAX_MESH_INSTANCES = 10000;
};

} // namespace astral
