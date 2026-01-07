#include "astral/renderer/gltf_loader.hpp"
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

namespace astral {

GltfLoader::GltfLoader(Context* context) : m_context(context) {
    createDefaultSampler();
}

GltfLoader::~GltfLoader() {
    for (auto sampler : m_samplers) {
        vkDestroySampler(m_context->getDevice(), sampler, nullptr);
    }
    vkDestroySampler(m_context->getDevice(), m_defaultSampler, nullptr);
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

void GltfLoader::createDefaultSampler() {
    VkSamplerCreateInfo samplerInfo = {VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};
    samplerInfo.magFilter = VK_FILTER_LINEAR;
    samplerInfo.minFilter = VK_FILTER_LINEAR;
    samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.anisotropyEnable = VK_TRUE;
    samplerInfo.maxAnisotropy = 16.0f;
    samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
    samplerInfo.unnormalizedCoordinates = VK_FALSE;
    samplerInfo.compareEnable = VK_FALSE;
    samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
    samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    samplerInfo.mipLodBias = 0.0f;
    samplerInfo.minLod = 0.0f;
    samplerInfo.maxLod = 0.0f;

    if (vkCreateSampler(m_context->getDevice(), &samplerInfo, nullptr, &m_defaultSampler) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create default sampler!");
    }
}

std::unique_ptr<Model> GltfLoader::loadFromFile(const std::filesystem::path& path, SceneManager* sceneManager) {
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

    fastgltf::Asset& asset = expectedAsset.get();
    auto model = std::make_unique<Model>();
    
    // 1. Sampler'ları Yükle
    for (auto& gltfSampler : asset.samplers) {
        VkSamplerCreateInfo samplerInfo = {VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};
        samplerInfo.magFilter = gltfSampler.magFilter.has_value() ? getVkFilter(gltfSampler.magFilter.value()) : VK_FILTER_LINEAR;
        samplerInfo.minFilter = gltfSampler.minFilter.has_value() ? getVkFilter(gltfSampler.minFilter.value()) : VK_FILTER_LINEAR;
        samplerInfo.mipmapMode = gltfSampler.minFilter.has_value() ? getVkMipmapMode(gltfSampler.minFilter.value()) : VK_SAMPLER_MIPMAP_MODE_LINEAR;
        samplerInfo.addressModeU = getVkWrapMode(gltfSampler.wrapS);
        samplerInfo.addressModeV = getVkWrapMode(gltfSampler.wrapT);
        samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        samplerInfo.anisotropyEnable = VK_TRUE;
        samplerInfo.maxAnisotropy = 16.0f;
        samplerInfo.maxLod = VK_LOD_CLAMP_NONE;

        VkSampler sampler;
        if (vkCreateSampler(m_context->getDevice(), &samplerInfo, nullptr, &sampler) != VK_SUCCESS) {
            spdlog::error("Failed to create glTF sampler!");
            continue;
        }
        m_samplers.push_back(sampler);
    }

    // 2. Image'ları Yükle (Ham veriler)
    std::vector<std::unique_ptr<Image>> loadedImages;
    loadedImages.reserve(asset.images.size());
    for (size_t i = 0; i < asset.images.size(); ++i) {
        auto& gltfImage = asset.images[i];
        std::unique_ptr<Image> image;
        
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
                
                int width, height, channels;
                stbi_uc* pixels = stbi_load(imagePath.string().c_str(), &width, &height, &channels, STBI_rgb_alpha);
                
                if (pixels) {
                    ImageSpecs specs;
                    specs.width = static_cast<uint32_t>(width);
                    specs.height = static_cast<uint32_t>(height);
                    specs.format = VK_FORMAT_R8G8B8A8_SRGB; 
                    specs.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
                    
                    image = std::make_unique<Image>(m_context, specs);
                    image->upload(pixels, specs.width * specs.height * 4);
                    stbi_image_free(pixels);
                    spdlog::info("Loaded image: {} ({}x{})", imagePath.string(), width, height);
                }
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
                            specs.format = VK_FORMAT_R8G8B8A8_SRGB;
                            specs.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
                            
                            image = std::make_unique<Image>(m_context, specs);
                            image->upload(pixels, specs.width * specs.height * 4);
                            stbi_image_free(pixels);
                            spdlog::info("Loaded image from BufferView ({}x{})", width, height);
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
            m_samplers[gltfTex.samplerIndex.value()] : m_defaultSampler;

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
            model->images.push_back(std::move(img));
        }
    }

    // 4. Materyal Yükleme
    std::vector<uint32_t> materialIndices;
    for (auto& gltfMat : asset.materials) {
        MaterialMetadata mat;
        mat.baseColorFactor = glm::make_vec4(gltfMat.pbrData.baseColorFactor.data());
        mat.metallicFactor = gltfMat.pbrData.metallicFactor;
        mat.roughnessFactor = gltfMat.pbrData.roughnessFactor;
        
        mat.baseColorTextureIndex = gltfMat.pbrData.baseColorTexture.has_value() ? 
            model->textureIndices[gltfMat.pbrData.baseColorTexture->textureIndex] : -1;
            
        mat.metallicRoughnessTextureIndex = gltfMat.pbrData.metallicRoughnessTexture.has_value() ? 
            model->textureIndices[gltfMat.pbrData.metallicRoughnessTexture->textureIndex] : -1;
            
        mat.normalTextureIndex = gltfMat.normalTexture.has_value() ? 
            model->textureIndices[gltfMat.normalTexture->textureIndex] : -1;
            
        mat.emissiveTextureIndex = gltfMat.emissiveTexture.has_value() ? 
            model->textureIndices[gltfMat.emissiveTexture->textureIndex] : -1;
            
        mat.occlusionTextureIndex = gltfMat.occlusionTexture.has_value() ? 
            model->textureIndices[gltfMat.occlusionTexture->textureIndex] : -1;

        materialIndices.push_back(sceneManager->addMaterial(mat));
    }

    // Default material if none exist
    if (materialIndices.empty()) {
        MaterialMetadata defaultMat;
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
                auto& accessor = asset.accessors[gltfPrimitive.indicesAccessor.value()];
                indices.reserve(indices.size() + accessor.count);
                
                fastgltf::iterateAccessor<uint32_t>(asset, accessor, [&](uint32_t index) {
                    indices.push_back(vertexStart + index);
                });
                primitive.indexCount = static_cast<uint32_t>(accessor.count);
            }

            // POSITION
            auto posAttr = gltfPrimitive.findAttribute("POSITION");
            if (posAttr != gltfPrimitive.attributes.end()) {
                auto& accessor = asset.accessors[posAttr->accessorIndex];
                vertices.resize(vertexStart + accessor.count);
                
                glm::vec3 minPos(std::numeric_limits<float>::max());
                glm::vec3 maxPos(std::numeric_limits<float>::lowest());

                fastgltf::iterateAccessorWithIndex<glm::vec3>(asset, accessor, [&](glm::vec3 pos, size_t idx) {
                    vertices[vertexStart + idx].position = pos;
                    minPos = glm::min(minPos, pos);
                    maxPos = glm::max(maxPos, pos);
                });

                primitive.boundingCenter = (minPos + maxPos) * 0.5f;
                primitive.boundingRadius = glm::distance(maxPos, primitive.boundingCenter);
            }

            // NORMAL
            auto normAttr = gltfPrimitive.findAttribute("NORMAL");
            if (normAttr != gltfPrimitive.attributes.end()) {
                auto& accessor = asset.accessors[normAttr->accessorIndex];
                fastgltf::iterateAccessorWithIndex<glm::vec3>(asset, accessor, [&](glm::vec3 norm, size_t idx) {
                    vertices[vertexStart + idx].normal = norm;
                });
            }

            // TEXCOORD_0
            auto uvAttr = gltfPrimitive.findAttribute("TEXCOORD_0");
            if (uvAttr != gltfPrimitive.attributes.end()) {
                auto& accessor = asset.accessors[uvAttr->accessorIndex];
                fastgltf::iterateAccessorWithIndex<glm::vec2>(asset, accessor, [&](glm::vec2 uv, size_t idx) {
                    vertices[vertexStart + idx].uv = uv;
                });
            }

            // TANGENT
            auto tangAttr = gltfPrimitive.findAttribute("TANGENT");
            if (tangAttr != gltfPrimitive.attributes.end()) {
                auto& accessor = asset.accessors[tangAttr->accessorIndex];
                fastgltf::iterateAccessorWithIndex<glm::vec4>(asset, accessor, [&](glm::vec4 tang, size_t idx) {
                    vertices[vertexStart + idx].tangent = tang;
                });
            }

            primitive.materialIndex = gltfPrimitive.materialIndex.has_value() ? 
                materialIndices[gltfPrimitive.materialIndex.value()] : materialIndices[0];
                
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
