#pragma once

#include "astral/core/context.hpp"
#include <vulkan/vulkan.h>

namespace astral {

struct SamplerSpecs {
    VkFilter magFilter = VK_FILTER_LINEAR;
    VkFilter minFilter = VK_FILTER_LINEAR;
    VkSamplerMipmapMode mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    VkSamplerAddressMode addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    VkSamplerAddressMode addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    VkSamplerAddressMode addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    bool anisotropyEnable = true;
    float maxAnisotropy = 16.0f;

    bool operator==(const SamplerSpecs& other) const {
        return magFilter == other.magFilter &&
               minFilter == other.minFilter &&
               mipmapMode == other.mipmapMode &&
               addressModeU == other.addressModeU &&
               addressModeV == other.addressModeV &&
               addressModeW == other.addressModeW &&
               anisotropyEnable == other.anisotropyEnable &&
               maxAnisotropy == other.maxAnisotropy;
    }
};

class Sampler {
public:
    Sampler(Context* context, const SamplerSpecs& specs);
    ~Sampler();

    VkSampler getHandle() const { return m_sampler; }
    const SamplerSpecs& getSpecs() const { return m_specs; }

private:
    Context* m_context;
    VkSampler m_sampler{VK_NULL_HANDLE};
    SamplerSpecs m_specs;
};

} // namespace astral

namespace std {
    template<> struct hash<astral::SamplerSpecs> {
        size_t operator()(const astral::SamplerSpecs& s) const {
            size_t h = 0;
            auto hash_combine = [&](size_t& seed, size_t v) {
                seed ^= v + 0x9e3779b9 + (seed << 6) + (seed >> 2);
            };
            
            hash_combine(h, std::hash<int>{}(s.magFilter));
            hash_combine(h, std::hash<int>{}(s.minFilter));
            hash_combine(h, std::hash<int>{}(s.mipmapMode));
            hash_combine(h, std::hash<int>{}(s.addressModeU));
            hash_combine(h, std::hash<int>{}(s.addressModeV));
            hash_combine(h, std::hash<int>{}(s.addressModeW));
            hash_combine(h, std::hash<bool>{}(s.anisotropyEnable));
            hash_combine(h, std::hash<float>{}(s.maxAnisotropy));
            
            return h;
        }
    };
}
