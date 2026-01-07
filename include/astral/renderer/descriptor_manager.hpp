#pragma once

#include "astral/core/context.hpp"
#include <vulkan/vulkan.h>
#include <vector>

namespace astral {

class DescriptorManager {
public:
    DescriptorManager(Context* context);
    ~DescriptorManager();

    VkDescriptorSetLayout getLayout() const { return m_layout; }
    VkDescriptorSet getDescriptorSet() const { return m_set; }

    uint32_t registerImage(VkImageView view, VkSampler sampler);
    uint32_t registerImageArray(VkImageView view, VkSampler sampler);
    uint32_t registerImageCube(VkImageView view, VkSampler sampler);
    uint32_t registerStorageImage(VkImageView view);
    uint32_t registerBuffer(VkBuffer buffer, VkDeviceSize offset, VkDeviceSize range, uint32_t binding = 1);

private:
    void createLayout();
    void createPoolAndSet();

    Context* m_context;
    VkDescriptorSetLayout m_layout;
    VkDescriptorPool m_pool;
    VkDescriptorSet m_set;

    uint32_t m_nextImageIndex = 0;
    uint32_t m_nextArrayImageIndex = 0;
    uint32_t m_nextCubeImageIndex = 0;
    uint32_t m_nextStorageImageIndex = 0;
    uint32_t m_nextBufferIndices[16]{0}; // Track indices per binding (extended for more buffer types)
    static constexpr uint32_t MAX_BINDLESS_IMAGES = 10000;
    static constexpr uint32_t MAX_BINDLESS_BUFFERS = 2000;
};

} // namespace astral
