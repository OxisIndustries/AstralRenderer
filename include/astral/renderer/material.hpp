#pragma once

#include <glm/glm.hpp>
#include <vulkan/vulkan.h>
#include <memory>
#include <string>
#include <vector>

#include "astral/resources/image.hpp"
#include "astral/resources/sampler.hpp"
// Forward decl for Pipeline if needed, or include appropriate header
// #include "astral/renderer/pipeline.hpp" 

namespace astral {

// Matches Shader Layout (std430 recommended for SSBO)
// Matches Shader Layout (std430 recommended for SSBO)
struct MaterialGPU {
    glm::vec4 baseColorFactor{1.0f};
    glm::vec4 emissiveFactor{0.0f, 0.0f, 0.0f, 1.0f}; // rgb: factor, a: strength

    float metallicFactor{1.0f};
    float roughnessFactor{1.0f};
    float alphaCutoff{0.5f};
    uint32_t alphaMode{0};      // 0=Opaque, 1=Mask, 2=Blend

    // Texture Indices (Bindless array indices)
    int32_t baseColorIndex{-1};
    int32_t normalIndex{-1};
    int32_t metallicRoughnessIndex{-1};
    int32_t emissiveIndex{-1};

    int32_t occlusionIndex{-1};
    uint32_t doubleSided{0};    // 0=False, 1=True
    uint32_t padding[2];        // Padding to align to 16-byte boundary. 
                                // Size: 16+16+4+4+4+4 + 4*4 + 4+4+8 = 80 bytes.
};

enum class AlphaMode : uint32_t {
    Opaque = 0,
    Mask = 1,
    Blend = 2
};

// CPU-side Material representation
// Holds the GPU data plus references to resources to keep them alive
struct Material {
    std::string name;
    MaterialGPU gpuData;
    
    // Resource references to ensure they stay alive while this material exists
    // (Actual GPU binding is done via indices in gpuData into the bindless arrays)
    std::shared_ptr<Image> baseColorTexture;
    std::shared_ptr<Image> normalTexture;
    std::shared_ptr<Image> metallicRoughnessTexture;
    std::shared_ptr<Image> emissiveTexture;
    std::shared_ptr<Image> occlusionTexture;
    
    // Samplers
    // In glTF, each texture can ideally have its own sampler.
    // We can store them here if we want to bind them specifically, 
    // but in our bindless model, we might bake the sampler into the descriptor 
    // or store sampler indices if using separate sampler array.
    // For now assuming combined image samplers registered in DescriptorManager.
};

} // namespace astral
