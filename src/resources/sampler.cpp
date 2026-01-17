#include "astral/resources/sampler.hpp"
#include <stdexcept>

namespace astral {

Sampler::Sampler(Context* context, const SamplerSpecs& specs) 
    : m_context(context), m_specs(specs) {
    
    VkSamplerCreateInfo samplerInfo = {VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};
    samplerInfo.magFilter = specs.magFilter;
    samplerInfo.minFilter = specs.minFilter;
    samplerInfo.mipmapMode = specs.mipmapMode;
    samplerInfo.addressModeU = specs.addressModeU;
    samplerInfo.addressModeV = specs.addressModeV;
    samplerInfo.addressModeW = specs.addressModeW;
    
    samplerInfo.anisotropyEnable = specs.anisotropyEnable ? VK_TRUE : VK_FALSE;
    samplerInfo.maxAnisotropy = specs.maxAnisotropy;
    
    // Defaults for stuff we don't expose yet but might need
    samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
    samplerInfo.unnormalizedCoordinates = VK_FALSE;
    samplerInfo.compareEnable = VK_FALSE;
    samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
    samplerInfo.mipLodBias = 0.0f;
    samplerInfo.minLod = 0.0f;
    samplerInfo.maxLod = VK_LOD_CLAMP_NONE;

    if (vkCreateSampler(m_context->getDevice(), &samplerInfo, nullptr, &m_sampler) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create sampler!");
    }
}

Sampler::~Sampler() {
    if (m_sampler != VK_NULL_HANDLE) {
        vkDestroySampler(m_context->getDevice(), m_sampler, nullptr);
    }
}

} // namespace astral
