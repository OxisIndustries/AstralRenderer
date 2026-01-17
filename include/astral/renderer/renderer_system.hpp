#pragma once

#include "astral/core/context.hpp"
#include "astral/renderer/compute_pipeline.hpp"
#include "astral/renderer/environment_manager.hpp"
#include "astral/renderer/model.hpp"
#include "astral/renderer/pipeline.hpp"
#include "astral/renderer/render_graph.hpp"
#include "astral/renderer/scene_data.hpp"
#include "astral/renderer/scene_manager.hpp"

#include <memory>
#include <vector>

namespace astral {

struct Model; // Forward declaration
class Swapchain;
class CommandBuffer;
class FrameSync;
class RendererSystem {
public:
  RendererSystem(Context *context, Swapchain *swapchain, uint32_t width,
                 uint32_t height);
  ~RendererSystem();

  struct UIParams {
    float exposure = 1.0f;
    float bloomStrength = 0.04f;
    float bloomThreshold = 1.0f;
    float bloomSoftness = 0.5f;
    bool showSkybox = true;
    bool enableFXAA = true;
    bool enableHeadlamp = false;
    bool enableSSAO = true;
    bool visualizeCascades = false;
    float shadowBias = 0.002f;
    float shadowNormalBias = 0.005f;
    int pcfRange = 2;
    float csmLambda = 0.95f;
    float ssaoRadius = 0.5f;
    float ssaoBias = 0.025f;
    float gamma = 2.2f;
    float iblIntensity = 1.0f;
    int selectedMaterial = 0;
    int selectedLight = 0;
  };

  void initializePipelines(VkDescriptorSetLayout *setLayouts,
                           uint32_t layoutCount);

  void render(CommandBuffer &cmd, RenderGraph &graph,
              SceneManager &sceneManager, uint32_t currentFrame,
              uint32_t imageIndex, const SceneData &sceneData,
              Swapchain *swapchain, FrameSync *sync, const UIParams &uiParams,
              const Model *model, uint32_t skyboxIndex);

  // Getters for resources that might be needed by App (or maybe App shouldn't
  // know) For now, let's keep it simple.

  // Helper to resize (recreate pipelines/images if needed)
  void onResize(uint32_t width, uint32_t height);

  // Accessors for registering resources
  void registerResources(DescriptorManager &descriptorManager);

  // Resources exposed for graph setup (maybe should be internal to render
  // method?) To keep it simple, we'll expose some necessary images or just
  // manage them internally and expose binding them to the graph.

  struct RenderResources {
    std::unique_ptr<Image> hdrImage;
    std::unique_ptr<Image> normalImage;
    std::unique_ptr<Image> depthImage;
    std::unique_ptr<Image> velocityImage;
    std::unique_ptr<Image> ldrImage;

    // TAA
    std::unique_ptr<Image> taaHistoryImage1;
    std::unique_ptr<Image> taaHistoryImage2;
    bool taaPingPong = false;

    // Shadow
    std::unique_ptr<Image> shadowImage;
    std::vector<VkImageView> shadowLayerViews;

    // SSAO
    std::unique_ptr<Image> ssaoImage;
    std::unique_ptr<Image> ssaoBlurImage;
    std::unique_ptr<Image> noiseImage;
    std::unique_ptr<Buffer> ssaoKernelBuffer;

    // Bloom
    std::unique_ptr<Image> bloomImage;
    std::unique_ptr<Image> bloomBlurImage;

    // Cluster
    std::unique_ptr<Buffer> clusterBuffer;
    std::vector<std::unique_ptr<Buffer>> clusterGridBuffers;
    std::vector<std::unique_ptr<Buffer>> lightIndexBuffers;
    std::vector<std::unique_ptr<Buffer>> clusterAtomicBuffers;
    std::vector<uint32_t> clusterGridBufferIndices;
    std::vector<uint32_t> lightIndexBufferIndices;
    std::vector<uint32_t> clusterAtomicBufferIndices;
  };

  RenderResources &getResources() { return m_resources; }

  // Method to setup the render graph for a frame
  void setupRenderGraph(RenderGraph &graph, Swapchain &swapchain,
                        uint32_t imageIndex, uint32_t currentFrame,
                        const SceneData &sceneData, const Model *model,
                        bool useTAA, bool showSkybox, bool enableFXAA,
                        bool enableSSAO);

private:
  Context *m_context;
  VkFormat m_swapchainFormat;
  uint32_t m_width;
  uint32_t m_height;

  // Shaders
  std::shared_ptr<Shader> m_vertShader;
  std::shared_ptr<Shader> m_fragShader;
  std::shared_ptr<Shader> m_postVertShader;
  std::shared_ptr<Shader> m_taaFragShader;
  std::shared_ptr<Shader> m_ssaoFragShader;
  std::shared_ptr<Shader> m_ssaoBlurFragShader;
  std::shared_ptr<Shader> m_compositeFragShader;
  std::shared_ptr<Shader> m_bloomFragShader;
  std::shared_ptr<Shader> m_fxaaFragShader;
  std::shared_ptr<Shader> m_shadowVertShader;
  std::shared_ptr<Shader> m_shadowFragShader;
  std::shared_ptr<Shader> m_cullShader;
  std::shared_ptr<Shader> m_clusterBuildShader;
  std::shared_ptr<Shader> m_clusterCullShader;
  std::shared_ptr<Shader> m_skyboxVertShader;
  std::shared_ptr<Shader> m_skyboxFragShader;

  // Pipelines
  std::unique_ptr<GraphicsPipeline> m_pbrPipeline; // Opaque
  std::unique_ptr<GraphicsPipeline> m_pbrTransparentPipeline; // Transparent
  std::unique_ptr<GraphicsPipeline> m_taaPipeline;
  std::unique_ptr<GraphicsPipeline> m_ssaoPipeline;
  std::unique_ptr<GraphicsPipeline> m_ssaoBlurPipeline;
  std::unique_ptr<GraphicsPipeline> m_compositePipeline;
  std::unique_ptr<GraphicsPipeline> m_bloomPipeline;
  std::unique_ptr<GraphicsPipeline> m_fxaaPipeline;
  std::unique_ptr<GraphicsPipeline> m_shadowPipeline;
  std::unique_ptr<ComputePipeline> m_cullPipeline;
  std::unique_ptr<ComputePipeline> m_clusterBuildPipeline;
  std::unique_ptr<ComputePipeline> m_clusterCullPipeline;
  std::unique_ptr<GraphicsPipeline> m_skyboxPipeline;

  // Layouts
  VkPipelineLayout m_pipelineLayout;
  VkPipelineLayout m_taaLayout;
  VkPipelineLayout m_ssaoLayout;
  VkPipelineLayout m_ssaoBlurLayout;
  VkPipelineLayout m_compositeLayout;
  VkPipelineLayout m_bloomLayout;
  VkPipelineLayout m_fxaaLayout;
  // m_shadowLayout reuses pipelineLayout (basic one) or we might need specific
  // if push constants differ
  VkPipelineLayout m_cullLayout;
  VkPipelineLayout m_clusterBuildLayout;
  VkPipelineLayout m_clusterCullLayout;
  VkPipelineLayout m_skyboxLayout;

  RenderResources m_resources;

  // Samplers
  VkSampler m_hdrSampler;
  VkSampler m_noiseSampler;
  VkSampler m_shadowSampler;

  // Indices
  uint32_t m_hdrTextureIndex;
  uint32_t m_normalTextureIndex;
  uint32_t m_depthTextureIndex;
  uint32_t m_velocityTextureIndex;
  uint32_t m_taaHistoryIndex1;
  uint32_t m_taaHistoryIndex2;
  uint32_t m_noiseTextureIndex;
  uint32_t m_ssaoTextureIndex;
  uint32_t m_ssaoBlurTextureIndex;
  uint32_t m_bloomTextureIndex;
  uint32_t m_bloomBlurTextureIndex;
  uint32_t m_shadowMapIndex;
  uint32_t m_ldrTextureIndex;
  uint32_t m_ssaoKernelBufferIndex;
  uint32_t m_clusterBufferIndex;

  bool m_clustersBuilt = false;

  // Internal helpers
  std::string readFile(const std::string &filename);
  void createSemaphores(); // Actually semaphores are per-frame, owned by App
                           // usually or Renderer? Sync object is in App.
};

} // namespace astral
