#include "astral/renderer/pipeline.hpp"
#include <stdexcept>

namespace astral {

GraphicsPipeline::GraphicsPipeline(Context* context, const PipelineSpecs& specs) : m_context(context) {
    std::vector<VkPipelineShaderStageCreateInfo> shaderStages;
    shaderStages.push_back(specs.vertexShader->getStageInfo());
    if (specs.fragmentShader) {
        shaderStages.push_back(specs.fragmentShader->getStageInfo());
    }

    VkPipelineVertexInputStateCreateInfo vertexInputInfo = {};
    vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexInputInfo.vertexBindingDescriptionCount = static_cast<uint32_t>(specs.vertexBindings.size());
    vertexInputInfo.pVertexBindingDescriptions = specs.vertexBindings.data();
    vertexInputInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(specs.vertexAttributes.size());
    vertexInputInfo.pVertexAttributeDescriptions = specs.vertexAttributes.data();

    VkPipelineInputAssemblyStateCreateInfo inputAssembly = {};
    inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

    VkPipelineViewportStateCreateInfo viewportState = {};
    viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportState.viewportCount = 1;
    viewportState.scissorCount = 1;

    VkPipelineRasterizationStateCreateInfo rasterizer = {};
    rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizer.polygonMode = specs.polygonMode;
    rasterizer.lineWidth = 1.0f;
    rasterizer.cullMode = specs.cullMode;
    rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;

    VkPipelineMultisampleStateCreateInfo multisampling = {};
    multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    VkPipelineDepthStencilStateCreateInfo depthStencil = {};
    depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depthStencil.depthTestEnable = specs.depthTest ? VK_TRUE : VK_FALSE;
    depthStencil.depthWriteEnable = specs.depthWrite ? VK_TRUE : VK_FALSE;
    depthStencil.depthCompareOp = specs.depthCompareOp;

    std::vector<VkPipelineColorBlendAttachmentState> colorBlendAttachments;
    for (const auto& format : specs.colorFormats) {
        VkPipelineColorBlendAttachmentState attachment = {};
        attachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
        attachment.blendEnable = specs.enableBlending ? VK_TRUE : VK_FALSE;
        
        attachment.srcColorBlendFactor = specs.srcColorBlendFactor;
        attachment.dstColorBlendFactor = specs.dstColorBlendFactor;
        attachment.colorBlendOp = VK_BLEND_OP_ADD;
        
        attachment.srcAlphaBlendFactor = specs.srcAlphaBlendFactor;
        attachment.dstAlphaBlendFactor = specs.dstAlphaBlendFactor;
        attachment.alphaBlendOp = VK_BLEND_OP_ADD;
        
        colorBlendAttachments.push_back(attachment);
    }

    VkPipelineColorBlendStateCreateInfo colorBlending = {};
    colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlending.attachmentCount = static_cast<uint32_t>(colorBlendAttachments.size());
    colorBlending.pAttachments = colorBlendAttachments.empty() ? nullptr : colorBlendAttachments.data();

    std::vector<VkDynamicState> dynamicStates = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
    VkPipelineDynamicStateCreateInfo dynamicState = {};
    dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamicState.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
    dynamicState.pDynamicStates = dynamicStates.data();

    // Vulkan 1.3 Dynamic Rendering
    VkPipelineRenderingCreateInfo renderingInfo = {};
    renderingInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
    renderingInfo.colorAttachmentCount = static_cast<uint32_t>(specs.colorFormats.size());
    renderingInfo.pColorAttachmentFormats = specs.colorFormats.empty() ? nullptr : specs.colorFormats.data();
    renderingInfo.depthAttachmentFormat = specs.depthFormat;

    VkGraphicsPipelineCreateInfo pipelineInfo = {};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineInfo.pNext = &renderingInfo;
    pipelineInfo.stageCount = static_cast<uint32_t>(shaderStages.size());
    pipelineInfo.pStages = shaderStages.data();
    pipelineInfo.pVertexInputState = &vertexInputInfo;
    pipelineInfo.pInputAssemblyState = &inputAssembly;
    pipelineInfo.pViewportState = &viewportState;
    pipelineInfo.pRasterizationState = &rasterizer;
    pipelineInfo.pMultisampleState = &multisampling;
    pipelineInfo.pDepthStencilState = &depthStencil;
    pipelineInfo.pColorBlendState = &colorBlending;
    pipelineInfo.pDynamicState = &dynamicState;
    pipelineInfo.layout = specs.layout;

    if (vkCreateGraphicsPipelines(m_context->getDevice(), VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &m_pipeline) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create graphics pipeline!");
    }
}

GraphicsPipeline::~GraphicsPipeline() {
    vkDestroyPipeline(m_context->getDevice(), m_pipeline, nullptr);
}

} // namespace astral
