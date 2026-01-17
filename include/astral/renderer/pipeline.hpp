#pragma once

#include "astral/core/context.hpp"
#include "astral/resources/shader.hpp"
#include <vulkan/vulkan.h>
#include <vector>
#include <memory>

namespace astral {

struct PipelineSpecs {
    std::shared_ptr<Shader> vertexShader;
    std::shared_ptr<Shader> fragmentShader;
    VkPipelineLayout layout;
    std::vector<VkFormat> colorFormats;
    VkFormat depthFormat = VK_FORMAT_UNDEFINED;
    
    std::vector<VkVertexInputBindingDescription> vertexBindings;
    std::vector<VkVertexInputAttributeDescription> vertexAttributes;

    bool depthTest = true;
    bool depthWrite = true;
    VkCompareOp depthCompareOp = VK_COMPARE_OP_LESS;
    VkCullModeFlags cullMode = VK_CULL_MODE_BACK_BIT;
    VkPolygonMode polygonMode = VK_POLYGON_MODE_FILL;

    bool enableBlending = false;
    VkBlendFactor srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
    VkBlendFactor dstColorBlendFactor = VK_BLEND_FACTOR_ZERO;
    VkBlendFactor srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    VkBlendFactor dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
};

class GraphicsPipeline {
public:
    GraphicsPipeline(Context* context, const PipelineSpecs& specs);
    ~GraphicsPipeline();

    VkPipeline getHandle() const { return m_pipeline; }

private:
    Context* m_context;
    VkPipeline m_pipeline;
};

} // namespace astral
