---
name: Vulkan
description: Best practices and guidelines for writing modern Vulkan 1.3+ code in AstralRenderer
---

# Vulkan Skill

This skill provides guidelines for writing Vulkan code within the AstralRenderer project. The project targets Vulkan 1.3 and uses modern features like Dynamic Rendering and Synchronization 2.

## Key Principles

### 1. Dynamic Rendering
**Do Not Use:** `VkRenderPass` or `VkFramebuffer`.
**Use:** `vkCmdBeginRendering` and `VkRenderingInfo`.

**Example:**
```cpp
VkRenderingAttachmentInfo colorAttachment{};
colorAttachment.imageView = swapchainImageView;
colorAttachment.imageLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL;
colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;

VkRenderingInfo renderingInfo{};
renderingInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
renderingInfo.renderArea = {{0, 0}, {width, height}};
renderingInfo.layerCount = 1;
renderingInfo.colorAttachmentCount = 1;
renderingInfo.pColorAttachments = &colorAttachment;

vkCmdBeginRendering(commandBuffer, &renderingInfo);
// ... draw commands ...
vkCmdEndRendering(commandBuffer);
```

### 2. Synchronization 2
**Do Not Use:** `vkCmdPipelineBarrier`.
**Use:** `vkCmdPipelineBarrier2` and `VkImageMemoryBarrier2`.

**Why:** It provides a more unified and readable interface for defining memory dependencies and pipeline stages.

**Example:**
```cpp
VkImageMemoryBarrier2 barrier{};
barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
barrier.srcStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
barrier.srcAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
barrier.dstStageMask = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT;
barrier.dstAccessMask = VK_ACCESS_2_SHADER_READ_BIT;
barrier.oldLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL;
barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
// ... image subresource range ...

VkDependencyInfo dependencyInfo{};
dependencyInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
dependencyInfo.imageMemoryBarrierCount = 1;
dependencyInfo.pImageMemoryBarriers = &barrier;

vkCmdPipelineBarrier2(commandBuffer, &dependencyInfo);
```

### 3. Bindless Rendering
**Overview:**
- **Set #0:** Global bindless textures (`descriptorIdx` keys).
- **Set #1:** Global uniform/storage buffers (Camera, Scene Data).
- **Set #2:** Material data (if applicable).

**Rule:** Always assume textures are accessed via an index into a global descriptor array, not individual bindings.

### 4. Memory Management (VMA)
**Requirement:** All GPU memory allocations MUST utilize **VulkanMemoryAllocator (VMA)**.
**Never:** Call `vkAllocateMemory` directly.

**Usage:**
```cpp
vmaCreateBuffer(allocator, &bufferCreateInfo, &allocCreateInfo, &buffer, &allocation, nullptr);
vmaCreateImage(allocator, &imageCreateInfo, &allocCreateInfo, &image, &allocation, nullptr);
```

### 5. Common Common Pitfalls
- **Layout Transitions:** Always transition images to the correct layout before use (e.g., `TRANSFER_DST` before copying, `SHADER_READ` before sampling).
- **Extensions:** Do not modify `Device::createLogicalDevice` without verifying if the extension is promoted to core Vulkan 1.3.
