#include "astral/renderer/gltf_loader.hpp"
#include "astral/renderer/asset_manager.hpp"
#include "astral/core/context.hpp"
#include "astral/renderer/scene_manager.hpp"
#include "astral/renderer/descriptor_manager.hpp"
#include <fastgltf/core.hpp>
#include <fastgltf/types.hpp>
#include <fastgltf/glm_element_traits.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <stb_image.h>
#include <spdlog/spdlog.h>
#include <fstream>
#include <memory>
#include <vulkan/vulkan.h>
#include "astral/resources/image.hpp"

namespace astral {

GltfLoader::GltfLoader(Context* context) : m_context(context) {
    // No default sampler creation
}

GltfLoader::~GltfLoader() {
    // Samplers managed by AssetManager
}

bool GltfLoader::supportsExtension(const std::string& extension) const {
    return extension == ".gltf" || extension == ".glb";
}

static VkFilter getVkFilter(fastgltf::Filter filter) {
    switch (filter) {
        case fastgltf::Filter::Nearest:
        case fastgltf::Filter::NearestMipMapNearest:
        case fastgltf::Filter::NearestMipMapLinear:
            return VK_FILTER_NEAREST;
        case fastgltf::Filter::Linear:
        case fastgltf::Filter::LinearMipMapNearest:
        case fastgltf::Filter::LinearMipMapLinear:
        default:
            return VK_FILTER_LINEAR;
    }
}

static VkSamplerMipmapMode getVkMipmapMode(fastgltf::Filter filter) {
    switch (filter) {
        case fastgltf::Filter::NearestMipMapNearest:
        case fastgltf::Filter::LinearMipMapNearest:
            return VK_SAMPLER_MIPMAP_MODE_NEAREST;
        case fastgltf::Filter::NearestMipMapLinear:
        case fastgltf::Filter::LinearMipMapLinear:
        default:
            return VK_SAMPLER_MIPMAP_MODE_LINEAR;
    }
}

static VkSamplerAddressMode getVkWrapMode(fastgltf::Wrap wrap) {
    switch (wrap) {
        case fastgltf::Wrap::Repeat: return VK_SAMPLER_ADDRESS_MODE_REPEAT;
        case fastgltf::Wrap::MirroredRepeat: return VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;
        case fastgltf::Wrap::ClampToEdge: return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        default: return VK_SAMPLER_ADDRESS_MODE_REPEAT;
    }
}

// createDefaultSampler removed

std::unique_ptr<Model> GltfLoader::load(const std::filesystem::path& path, SceneManager* sceneManager, AssetManager* assetManager) {
    if (!std::filesystem::exists(path)) {
        spdlog::error("glTF file not found: {}", path.string());
        return nullptr;
    }

    static constexpr auto options = fastgltf::Options::DontRequireValidAssetMember |
                                    fastgltf::Options::LoadExternalBuffers;

    fastgltf::Parser parser;
    auto data = fastgltf::GltfDataBuffer::FromPath(path);
    if (data.error() != fastgltf::Error::None) {
        spdlog::error("Failed to load glTF data buffer: {}", static_cast<uint64_t>(data.error()));
        return nullptr;
    }

    auto expectedAsset = parser.loadGltf(data.get(), path.parent_path(), options);
    if (expectedAsset.error() != fastgltf::Error::None) {
        spdlog::error("Failed to parse glTF: {}", static_cast<uint64_t>(expectedAsset.error()));
        return nullptr;
    }

    auto& asset = expectedAsset.get();
    auto model = std::make_unique<Model>();
    
    // 1. Sampler'ları Yükle
    // 1. Sampler'ları Yükle
    std::vector<VkSampler> loadedSamplers;
    loadedSamplers.reserve(asset.samplers.size());
    
    for (auto& gltfSampler : asset.samplers) {
        SamplerSpecs specs;
        specs.magFilter = gltfSampler.magFilter.has_value() ? getVkFilter(gltfSampler.magFilter.value()) : VK_FILTER_LINEAR;
        specs.minFilter = gltfSampler.minFilter.has_value() ? getVkFilter(gltfSampler.minFilter.value()) : VK_FILTER_LINEAR;
        specs.mipmapMode = gltfSampler.minFilter.has_value() ? getVkMipmapMode(gltfSampler.minFilter.value()) : VK_SAMPLER_MIPMAP_MODE_LINEAR;
        specs.addressModeU = getVkWrapMode(gltfSampler.wrapS);
        specs.addressModeV = getVkWrapMode(gltfSampler.wrapT);
        specs.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        specs.anisotropyEnable = true;
        specs.maxAnisotropy = 16.0f;

        loadedSamplers.push_back(assetManager->getSampler(specs));
    }
    
    SamplerSpecs defaultSpecs;
    VkSampler defaultSampler = assetManager->getSampler(defaultSpecs);

    // 2. Image'ları Yükle (Ham veriler)
    // 2.1 USAGE Pre-pass to determine fallback types and formats
    std::vector<TextureType> imageTypes(asset.images.size(), TextureType::Albedo);
    for (auto& gltfMat : asset.materials) {
        auto checkTex = [&](auto& info, TextureType type) {
            if (info.has_value()) {
                auto& tex = asset.textures[info.value().textureIndex];
                if (tex.imageIndex.has_value()) {
                    imageTypes[tex.imageIndex.value()] = type;
                }
            }
        };
        checkTex(gltfMat.pbrData.baseColorTexture, TextureType::Albedo);
        checkTex(gltfMat.pbrData.metallicRoughnessTexture, TextureType::MetallicRoughness);
        checkTex(gltfMat.normalTexture, TextureType::Normal);
        checkTex(gltfMat.occlusionTexture, TextureType::Occlusion);
        checkTex(gltfMat.emissiveTexture, TextureType::Emissive);
        
        if (gltfMat.transmission) {
            checkTex(gltfMat.transmission->transmissionTexture, TextureType::Transmission);
        }
        if (gltfMat.volume) {
            checkTex(gltfMat.volume->thicknessTexture, TextureType::Thickness);
        }
    }

    std::vector<std::shared_ptr<Image>> loadedImages;
    loadedImages.reserve(asset.images.size());
    for (size_t i = 0; i < asset.images.size(); ++i) {
        auto& gltfImage = asset.images[i];
        std::shared_ptr<Image> image;
        TextureType type = imageTypes[i];
        
        std::visit(fastgltf::visitor {
            [&](fastgltf::sources::URI& uri) {
                if (uri.fileByteOffset != 0) {
                    spdlog::warn("URI with offset not supported yet: image index {}", i);
                    return;
                }
                
                std::filesystem::path imagePath;
                if (uri.uri.scheme() == "file") {
                    imagePath = uri.uri.fspath();
                } else if (uri.uri.scheme().empty()) {
                    imagePath = path.parent_path() / uri.uri.fspath();
                } else {
                    spdlog::warn("Unsupported URI scheme: {} for image index {}", uri.uri.scheme(), i);
                    return;
                }

                // Use AssetManager for caching with Type awareness!
                image = assetManager->getOrLoadTexture(imagePath, type);
            },
            [&](fastgltf::sources::BufferView& view) {
                auto& bufferView = asset.bufferViews[view.bufferViewIndex];
                auto& buffer = asset.buffers[bufferView.bufferIndex];
                
                std::visit(fastgltf::visitor {
                    [&](fastgltf::sources::Array& array) {
                        int width, height, channels;
                        stbi_uc* pixels = stbi_load_from_memory(
                            reinterpret_cast<const stbi_uc*>(array.bytes.data() + bufferView.byteOffset),
                            static_cast<int>(bufferView.byteLength),
                            &width, &height, &channels, STBI_rgb_alpha
                        );
                        
                        if (pixels) {
                            ImageSpecs specs;
                            specs.width = static_cast<uint32_t>(width);
                            specs.height = static_cast<uint32_t>(height);
                            // Set format based on type
                            specs.format = (type == TextureType::Normal) ? 
                                VK_FORMAT_R8G8B8A8_UNORM : VK_FORMAT_R8G8B8A8_SRGB;
                                
                            specs.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
                            
                            image = std::make_shared<Image>(m_context, specs);
                            image->upload(pixels, specs.width * specs.height * 4);
                            stbi_image_free(pixels);
                            spdlog::info("Loaded image from BufferView ({}x{}) as {}", width, height, 
                                (type == TextureType::Normal ? "UNORM" : "SRGB"));
                        }
                    },
                    [&](auto&) {}
                }, buffer.data);
            },
            [&](auto&) {}
        }, gltfImage.data);
        
        loadedImages.push_back(std::move(image));
    }

    // 3. Texture'ları Yükle (Image + Sampler kombinasyonları)
    model->textureIndices.reserve(asset.textures.size());
    for (size_t i = 0; i < asset.textures.size(); ++i) {
        auto& gltfTex = asset.textures[i];
        if (!gltfTex.imageIndex.has_value()) {
            model->textureIndices.push_back(0); // Default fallback
            continue;
        }

        uint32_t imgIdx = static_cast<uint32_t>(gltfTex.imageIndex.value());
        VkSampler sampler = gltfTex.samplerIndex.has_value() ? 
            loadedSamplers[gltfTex.samplerIndex.value()] : defaultSampler;

        if (imgIdx < loadedImages.size() && loadedImages[imgIdx]) {
            uint32_t texIdx = m_context->getDescriptorManager().registerImage(loadedImages[imgIdx]->getView(), sampler);
            model->textureIndices.push_back(texIdx);
            spdlog::debug("Registered texture {} using image {}", i, imgIdx);
        } else {
            spdlog::warn("Texture {} references missing or moved image {}", i, imgIdx);
            model->textureIndices.push_back(0);
        }
    }

    // Move images to model at the very end
    for (auto& img : loadedImages) {
        if (img) {
            model->images.push_back(img); // Shared ptr copy
        }
    }

    // 4. Materyal Yükleme
    std::vector<int32_t> materialIndices;
    for (auto& gltfMat : asset.materials) {
        Material material;
        material.name = gltfMat.name.c_str();

        // Base Color
        material.gpuData.baseColorFactor = glm::make_vec4(gltfMat.pbrData.baseColorFactor.data());
        if (gltfMat.pbrData.baseColorTexture.has_value()) {
            size_t texIdx = gltfMat.pbrData.baseColorTexture->textureIndex;
            material.gpuData.baseColorIndex = model->textureIndices[texIdx];
            // Keep ref
            if (model->images.size() > texIdx) {
                // Warning: model->images might not index 1:1 with textures map in gltf if some textures failed
                // But generally model->images corresponds to loadedImages which maps to unique images.
                // gltfMat textures index into asset.textures. 
                // We populated model->textureIndices from asset.textures.
                // We need the corresponding Image pointer to store in Material host struct.
                // The gltfTex ref has imageIndex. 
                auto& gltfTex = asset.textures[texIdx];
                 if (gltfTex.imageIndex.has_value()) {
                     size_t imgIdx = gltfTex.imageIndex.value();
                     if (imgIdx < loadedImages.size()) {
                         material.baseColorTexture = loadedImages[imgIdx];
                     }
                 }
            }
        }

        // Metallic Roughness
        material.gpuData.metallicFactor = gltfMat.pbrData.metallicFactor;
        material.gpuData.roughnessFactor = gltfMat.pbrData.roughnessFactor;
        if (gltfMat.pbrData.metallicRoughnessTexture.has_value()) {
            size_t texIdx = gltfMat.pbrData.metallicRoughnessTexture->textureIndex;
            material.gpuData.metallicRoughnessIndex = model->textureIndices[texIdx];
            
             auto& gltfTex = asset.textures[texIdx];
             if (gltfTex.imageIndex.has_value()) {
                 size_t imgIdx = gltfTex.imageIndex.value();
                 if (imgIdx < loadedImages.size()) material.metallicRoughnessTexture = loadedImages[imgIdx];
             }
        }

        // Normal
        if (gltfMat.normalTexture.has_value()) {
            size_t texIdx = gltfMat.normalTexture->textureIndex;
            material.gpuData.normalIndex = model->textureIndices[texIdx];
            
             auto& gltfTex = asset.textures[texIdx];
             if (gltfTex.imageIndex.has_value()) {
                 size_t imgIdx = gltfTex.imageIndex.value();
                 if (imgIdx < loadedImages.size()) material.normalTexture = loadedImages[imgIdx];
             }
        }

        // Emissive
        material.gpuData.emissiveFactor = glm::vec4(gltfMat.emissiveFactor[0], gltfMat.emissiveFactor[1], gltfMat.emissiveFactor[2], 1.0f);
        
        if (gltfMat.emissiveTexture.has_value()) {
            size_t texIdx = gltfMat.emissiveTexture->textureIndex;
            material.gpuData.emissiveIndex = model->textureIndices[texIdx];
            
             auto& gltfTex = asset.textures[texIdx];
             if (gltfTex.imageIndex.has_value()) {
                 size_t imgIdx = gltfTex.imageIndex.value();
                 if (imgIdx < loadedImages.size()) material.emissiveTexture = loadedImages[imgIdx];
             }
        }
        
        // Occlusion
        if (gltfMat.occlusionTexture.has_value()) {
            size_t texIdx = gltfMat.occlusionTexture->textureIndex;
            material.gpuData.occlusionIndex = model->textureIndices[texIdx];
            
             auto& gltfTex = asset.textures[texIdx];
             if (gltfTex.imageIndex.has_value()) {
                 size_t imgIdx = gltfTex.imageIndex.value();
                 if (imgIdx < loadedImages.size()) material.occlusionTexture = loadedImages[imgIdx];
             }
        }
        
        // Transmission
        if (gltfMat.transmission) {
            auto& trans = *gltfMat.transmission;
            material.gpuData.transmissionFactor = trans.transmissionFactor;
            
            if (trans.transmissionTexture.has_value()) {
                size_t texIdx = trans.transmissionTexture->textureIndex;
                material.gpuData.transmissionIndex = model->textureIndices[texIdx];
                
                auto& gltfTex = asset.textures[texIdx];
                if (gltfTex.imageIndex.has_value()) {
                    size_t imgIdx = gltfTex.imageIndex.value();
                    if (imgIdx < loadedImages.size()) {
                        material.transmissionTexture = loadedImages[imgIdx];
                    }
                }
            }
        }

        // IOR
        material.gpuData.ior = gltfMat.ior;

        // Volume
        if (gltfMat.volume) {
            auto& vol = *gltfMat.volume;
            material.gpuData.thicknessFactor = vol.thicknessFactor;
            
            if (vol.thicknessTexture.has_value()) {
                size_t texIdx = vol.thicknessTexture->textureIndex;
                material.gpuData.thicknessIndex = model->textureIndices[texIdx];
                
                auto& gltfTex = asset.textures[texIdx];
                if (gltfTex.imageIndex.has_value()) {
                    size_t imgIdx = gltfTex.imageIndex.value();
                    if (imgIdx < loadedImages.size()) {
                        material.thicknessTexture = loadedImages[imgIdx];
                    }
                }
            }
        }

        if (gltfMat.alphaMode == fastgltf::AlphaMode::Mask) {
            material.gpuData.alphaMode = static_cast<uint32_t>(AlphaMode::Mask);
            material.gpuData.alphaCutoff = gltfMat.alphaCutoff;
        } else if (gltfMat.alphaMode == fastgltf::AlphaMode::Blend) {
            material.gpuData.alphaMode = static_cast<uint32_t>(AlphaMode::Blend);
        } else {
            material.gpuData.alphaMode = static_cast<uint32_t>(AlphaMode::Opaque);
        }
        
        material.gpuData.doubleSided = gltfMat.doubleSided ? 1 : 0;

        materialIndices.push_back(sceneManager->addMaterial(material));
    }

    // Default material if none exist
    if (materialIndices.empty()) {
        Material defaultMat;
        defaultMat.name = "Default";
        materialIndices.push_back(sceneManager->addMaterial(defaultMat));
    }

    // 3. Geometri Yükleme
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;

    for (size_t meshIdx = 0; meshIdx < asset.meshes.size(); ++meshIdx) {
        auto& gltfMesh = asset.meshes[meshIdx];
        Mesh mesh;
        mesh.name = gltfMesh.name.c_str();

        for (size_t primIdx = 0; primIdx < gltfMesh.primitives.size(); ++primIdx) {
            auto& gltfPrimitive = gltfMesh.primitives[primIdx];
            Primitive primitive;
            primitive.firstIndex = static_cast<uint32_t>(indices.size());
            uint32_t vertexStart = static_cast<uint32_t>(vertices.size());

            // Index verilerini oku
            if (gltfPrimitive.indicesAccessor.has_value()) {
                size_t accIdx = gltfPrimitive.indicesAccessor.value();
                indices.reserve(indices.size() + asset.accessors[accIdx].count);
                
                fastgltf::iterateAccessor<uint32_t>(asset, asset.accessors[accIdx], [&](uint32_t index) {
                    indices.push_back(vertexStart + index);
                });
                primitive.indexCount = static_cast<uint32_t>(asset.accessors[accIdx].count);
            }

            // POSITION
            {
                auto posIt = gltfPrimitive.findAttribute("POSITION");
                if (posIt != gltfPrimitive.attributes.end()) {
                    size_t accIdx = posIt->accessorIndex;
                    vertices.resize(vertexStart + asset.accessors[accIdx].count);
                    
                    glm::vec3 minPos(std::numeric_limits<float>::max());
                    glm::vec3 maxPos(std::numeric_limits<float>::lowest());
    
                    fastgltf::iterateAccessorWithIndex<glm::vec3>(asset, asset.accessors[accIdx], [&](glm::vec3 pos, size_t idx) {
                        vertices[vertexStart + idx].position = pos;
                        minPos = glm::min(minPos, pos);
                        maxPos = glm::max(maxPos, pos);
                    });
    
                    primitive.boundingCenter = (minPos + maxPos) * 0.5f;
                    primitive.boundingRadius = glm::distance(maxPos, primitive.boundingCenter);
                }
            }
 
            // NORMAL
            {
                auto normIt = gltfPrimitive.findAttribute("NORMAL");
                if (normIt != gltfPrimitive.attributes.end()) {
                    size_t accIdx = normIt->accessorIndex;
                    fastgltf::iterateAccessorWithIndex<glm::vec3>(asset, asset.accessors[accIdx], [&](glm::vec3 norm, size_t idx) {
                        vertices[vertexStart + idx].normal = norm;
                    });
                }
            }
 
            // TEXCOORD_0
            {
                auto uvIt = gltfPrimitive.findAttribute("TEXCOORD_0");
                if (uvIt != gltfPrimitive.attributes.end()) {
                    size_t accIdx = uvIt->accessorIndex;
                    fastgltf::iterateAccessorWithIndex<glm::vec2>(asset, asset.accessors[accIdx], [&](glm::vec2 uv, size_t idx) {
                        vertices[vertexStart + idx].uv = uv;
                    });
                }
            }
 
            // TANGENT
            {
                auto tangIt = gltfPrimitive.findAttribute("TANGENT");
                if (tangIt != gltfPrimitive.attributes.end()) {
                    size_t accIdx = tangIt->accessorIndex;
                    fastgltf::iterateAccessorWithIndex<glm::vec4>(asset, asset.accessors[accIdx], [&](glm::vec4 tang, size_t idx) {
                        vertices[vertexStart + idx].tangent = tang;
                    });
                }
            }

            if (gltfPrimitive.materialIndex.has_value()) {
                size_t matIdx = gltfPrimitive.materialIndex.value();
                primitive.materialIndex = materialIndices[matIdx];
            } else {
                primitive.materialIndex = materialIndices[0];
            }
                
            mesh.primitives.push_back(primitive);
        }
        model->meshes.push_back(mesh);
    }

    // GPU Buffer'larını yarat
    model->vertexBuffer = std::make_unique<Buffer>(
        m_context,
        vertices.size() * sizeof(Vertex),
        VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
        VMA_MEMORY_USAGE_CPU_TO_GPU
    );
    model->vertexBuffer->upload(vertices.data(), vertices.size() * sizeof(Vertex));

    model->indexBuffer = std::make_unique<Buffer>(
        m_context,
        indices.size() * sizeof(uint32_t),
        VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
        VMA_MEMORY_USAGE_CPU_TO_GPU
    );
    model->indexBuffer->upload(indices.data(), indices.size() * sizeof(uint32_t));

    spdlog::info("glTF model loaded: {} meshes, {} materials, {} textures", 
                 model->meshes.size(), materialIndices.size(), model->images.size());
    return model;
}

} // namespace astral
