#include "astral/renderer/render_graph.hpp"
#include <spdlog/spdlog.h>

namespace astral {

RenderGraph::RenderGraph(Context* context) : m_context(context) {}

RenderGraph::~RenderGraph() {}

void RenderGraph::addPass(const std::string& name, 
                         const std::vector<std::string>& inputs, 
                         const std::vector<std::string>& outputs, 
                         RenderPassExecuteCallback execute,
                         bool clearOutputs) {
    m_passes.push_back({name, inputs, outputs, execute, clearOutputs});
}

void RenderGraph::addExternalResource(const std::string& name, VkImage image, VkImageView view, VkFormat format, uint32_t width, uint32_t height, VkImageLayout initialLayout) {
    RenderPassResource res;
    res.name = name;
    res.image = image;
    res.view = view;
    res.format = format;
    res.width = width;
    res.height = height;
    res.isExternal = true;
    res.currentLayout = initialLayout;
    res.clearValue.color = {{0.0f, 0.0f, 0.0f, 1.0f}};
    m_resources[name] = res;
    m_imageLayouts[image] = initialLayout;
}

void RenderGraph::setResourceClearValue(const std::string& name, VkClearValue clearValue) {
    if (m_resources.find(name) != m_resources.end()) {
        m_resources[name].clearValue = clearValue;
    }
}

void RenderGraph::execute(VkCommandBuffer cmd, VkExtent2D extent) {
    for (size_t i = 0; i < m_passes.size(); ++i) {
        const auto& pass = m_passes[i];
        bool isLastPass = (i == m_passes.size() - 1);

        std::vector<VkImageMemoryBarrier2> barriers;

        // Handle Inputs (Transition to Shader Read)
        for (const auto& inputName : pass.inputs) {
            auto& res = m_resources[inputName];
            if (m_imageLayouts[res.image] != VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
                bool isDepth = (res.format == VK_FORMAT_D32_SFLOAT || res.format == VK_FORMAT_D32_SFLOAT_S8_UINT || res.format == VK_FORMAT_D24_UNORM_S8_UINT);
                
                VkImageMemoryBarrier2 barrier = {VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2};
                barrier.image = res.image;
                
                if (isDepth) {
                    barrier.srcStageMask = VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT;
                    barrier.srcAccessMask = VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
                    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
                } else {
                    barrier.srcStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
                    barrier.srcAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
                    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
                }
                
                barrier.dstStageMask = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT;
                barrier.dstAccessMask = VK_ACCESS_2_SHADER_READ_BIT;
                barrier.oldLayout = m_imageLayouts[res.image];
                barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
                barrier.subresourceRange.baseMipLevel = 0;
                barrier.subresourceRange.levelCount = VK_REMAINING_MIP_LEVELS;
                barrier.subresourceRange.baseArrayLayer = 0;
                barrier.subresourceRange.layerCount = VK_REMAINING_ARRAY_LAYERS;
                barriers.push_back(barrier);
                m_imageLayouts[res.image] = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
                res.currentLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            }
        }

        // Handle Outputs (Transition to Color/Depth Attachment)
        for (const auto& outName : pass.outputs) {
            auto& res = m_resources[outName];
            bool isDepth = (res.format == VK_FORMAT_D32_SFLOAT || res.format == VK_FORMAT_D32_SFLOAT_S8_UINT || res.format == VK_FORMAT_D24_UNORM_S8_UINT);
            VkImageLayout targetLayout = isDepth ? VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL : VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

            if (m_imageLayouts[res.image] != targetLayout) {
                VkImageMemoryBarrier2 barrier = {VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2};
                barrier.image = res.image;
                barrier.srcStageMask = VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT;
                barrier.srcAccessMask = 0;
                barrier.dstStageMask = isDepth ? VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT : VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
                barrier.dstAccessMask = isDepth ? VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT : VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
                barrier.oldLayout = m_imageLayouts[res.image];
                barrier.newLayout = targetLayout;
                barrier.subresourceRange.aspectMask = isDepth ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT;
                barrier.subresourceRange.baseMipLevel = 0;
                barrier.subresourceRange.levelCount = VK_REMAINING_MIP_LEVELS;
                barrier.subresourceRange.baseArrayLayer = 0;
                barrier.subresourceRange.layerCount = VK_REMAINING_ARRAY_LAYERS;
                barriers.push_back(barrier);
                m_imageLayouts[res.image] = targetLayout;
                res.currentLayout = targetLayout;
            }
        }

        if (!barriers.empty()) {
            VkDependencyInfo depInfo = {VK_STRUCTURE_TYPE_DEPENDENCY_INFO};
            depInfo.imageMemoryBarrierCount = static_cast<uint32_t>(barriers.size());
            depInfo.pImageMemoryBarriers = barriers.data();
            vkCmdPipelineBarrier2(cmd, &depInfo);
        }

        // Prepare Attachments
        std::vector<VkRenderingAttachmentInfo> colorAttachments;
        VkRenderingAttachmentInfo depthAttachment = {VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO};
        bool hasDepth = false;

        for (const auto& outName : pass.outputs) {
            auto& res = m_resources[outName];
            
            bool isDepth = (res.format == VK_FORMAT_D32_SFLOAT || res.format == VK_FORMAT_D32_SFLOAT_S8_UINT || res.format == VK_FORMAT_D24_UNORM_S8_UINT);
            
            VkRenderingAttachmentInfo attachment = {VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO};
            attachment.imageView = res.view;
            attachment.imageLayout = isDepth ? VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL : VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
            attachment.loadOp = pass.clearOutputs ? VK_ATTACHMENT_LOAD_OP_CLEAR : VK_ATTACHMENT_LOAD_OP_LOAD;
            attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
            attachment.clearValue = res.clearValue;

            if (isDepth) {
                depthAttachment = attachment;
                hasDepth = true;
            } else {
                colorAttachments.push_back(attachment);
            }
        }

        if (!pass.outputs.empty()) {
            VkRenderingInfo renderingInfo = {VK_STRUCTURE_TYPE_RENDERING_INFO};
            const auto& firstOut = m_resources[pass.outputs[0]];
            renderingInfo.renderArea = {{0, 0}, {firstOut.width, firstOut.height}};
            renderingInfo.layerCount = 1;
            renderingInfo.colorAttachmentCount = static_cast<uint32_t>(colorAttachments.size());
            renderingInfo.pColorAttachments = colorAttachments.data();
            if (hasDepth) {
                renderingInfo.pDepthAttachment = &depthAttachment;
            }

            spdlog::debug("RenderGraph: Executing pass '{}' with {} color attachments, hasDepth={}",
                          pass.name, colorAttachments.size(), hasDepth);

            vkCmdBeginRendering(cmd, &renderingInfo);
            pass.execute(cmd);
            vkCmdEndRendering(cmd);
        } else {
            // Compute pass or pass with no attachments
            pass.execute(cmd);
        }

        // If it's the last pass and output is external (swapchain), transition to Present
        if (isLastPass) {
            std::vector<VkImageMemoryBarrier2> finalBarriers;
            for (const auto& outName : pass.outputs) {
                auto& res = m_resources[outName];
                if (res.isExternal) {
                    bool isDepth = (res.format == VK_FORMAT_D32_SFLOAT || res.format == VK_FORMAT_D32_SFLOAT_S8_UINT || res.format == VK_FORMAT_D24_UNORM_S8_UINT);
                    if (isDepth) continue; // Don't present depth

                    VkImageMemoryBarrier2 barrier = {VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2};
                    barrier.image = res.image;
                    barrier.srcStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
                    barrier.srcAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
                    barrier.dstStageMask = VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT;
                    barrier.dstAccessMask = 0;
                    barrier.oldLayout = m_imageLayouts[res.image];
                    barrier.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
                    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
                    barrier.subresourceRange.baseMipLevel = 0;
                    barrier.subresourceRange.levelCount = VK_REMAINING_MIP_LEVELS;
                    barrier.subresourceRange.baseArrayLayer = 0;
                    barrier.subresourceRange.layerCount = VK_REMAINING_ARRAY_LAYERS;
                    finalBarriers.push_back(barrier);
                    m_imageLayouts[res.image] = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
                    res.currentLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
                }
            }

            if (!finalBarriers.empty()) {
                VkDependencyInfo depInfo = {VK_STRUCTURE_TYPE_DEPENDENCY_INFO};
                depInfo.imageMemoryBarrierCount = static_cast<uint32_t>(finalBarriers.size());
                depInfo.pImageMemoryBarriers = finalBarriers.data();
                vkCmdPipelineBarrier2(cmd, &depInfo);
            }
        }
    }
}

void RenderGraph::clear() {
    m_passes.clear();
    
    // Clear internal resources and their layout tracking
    auto it = m_resources.begin();
    while (it != m_resources.end()) {
        if (!it->second.isExternal) {
            m_imageLayouts.erase(it->second.image);
            it = m_resources.erase(it);
        } else {
            ++it;
        }
    }
}

} // namespace astral
