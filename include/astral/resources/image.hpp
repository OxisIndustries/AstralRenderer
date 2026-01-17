#pragma once

#include "astral/core/context.hpp"
#include <vk_mem_alloc.h>

namespace astral {

struct ImageSpecs {
    uint32_t width;
    uint32_t height;
    uint32_t depth = 1;
    VkFormat format;
    VkImageUsageFlags usage;
    VmaMemoryUsage memoryUsage = VMA_MEMORY_USAGE_AUTO;
    VkImageAspectFlags aspectFlags = VK_IMAGE_ASPECT_COLOR_BIT;
    uint32_t mipLevels = 1;
    uint32_t arrayLayers = 1;
    VkSampleCountFlagBits samples = VK_SAMPLE_COUNT_1_BIT;
    VkImageType type = VK_IMAGE_TYPE_2D;
    VkImageViewType viewType = VK_IMAGE_VIEW_TYPE_2D;
};

class Image {
public:
    Image(Context* context, const ImageSpecs& specs);
    ~Image();

    // Disable copying
    Image(const Image&) = delete;
    Image& operator=(const Image&) = delete;

    VkImage getHandle() const { return m_image; }
    VkImageView getView() const { return m_view; }
    const ImageSpecs& getSpecs() const { return m_specs; }

    void upload(const void* data, VkDeviceSize size);

private:
    Context* m_context;
    ImageSpecs m_specs;
    VkImage m_image;
    VmaAllocation m_allocation;
    VkImageView m_view;

    void createView();
    void generateMipmaps(VkCommandBuffer cmd);
};

} // namespace astral
