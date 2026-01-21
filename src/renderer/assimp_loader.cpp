#include "astral/renderer/assimp_loader.hpp"
#include "astral/renderer/asset_manager.hpp"
#include "astral/core/context.hpp"
#include "astral/renderer/scene_manager.hpp"
#include "astral/renderer/descriptor_manager.hpp"
#include <stb_image.h>
#include <spdlog/spdlog.h>
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <memory>
#include <vulkan/vulkan.h>
#include "astral/resources/image.hpp"

namespace astral {

class AssimpLoader; // forward decl if needed


AssimpLoader::AssimpLoader(Context* context) : m_context(context) {
    // No default sampler creation needed here
}

AssimpLoader::~AssimpLoader() {
    // Samplers managed by AssetManager
}

// createDefaultSampler removed

bool AssimpLoader::supportsExtension(const std::string& extension) const {
    // Support common formats
    return extension == ".obj" || extension == ".fbx" || extension == ".dae" || extension == ".blend";
}

std::unique_ptr<Model> AssimpLoader::load(const std::filesystem::path& path, SceneManager* sceneManager, AssetManager* assetManager) {
    Assimp::Importer importer;
    
    // Flags:
    // aiProcess_Triangulate: Ensure all faces are triangles
    // aiProcess_FlipUVs: Flip Texture coordinates (Vulkan uses top-left origin for logic? No, Vulkan usually needs flip if OpenGL style. Assimp default is bottom-left. Vulkan UV is top-left. But typically standard models expect OpenGL style so we often flip. Let's stick to standard practice, check if GltfLoader does it. GltfLoader didn't explicit flip, usually glTF is top-left in spec? No, glTF is top-left. OBJ is bottom-left. Assimp normalizes. Let's try without FlipUVs first (Assimp default) or with it if textures are upside down. Wait, OpenGL and glTF coordinate systems differ.
    // aiProcess_CalcTangentSpace: For normal mapping
    // aiProcess_GenSmoothNormals: If normals missing
    // aiProcess_PreTransformVertices: Flatten hierarchy (simplifies rendering loop significantly)
    
    const aiScene* scene = importer.ReadFile(path.string(), 
        aiProcess_Triangulate | 
        aiProcess_GenSmoothNormals | 
        aiProcess_CalcTangentSpace |
        aiProcess_PreTransformVertices |
        aiProcess_JoinIdenticalVertices |
        aiProcess_FlipUVs |              // FBX uses bottom-left UV origin, Vulkan uses top-left
        aiProcess_TransformUVCoords      // Apply any UV transforms embedded in the file
    );

    if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode) {
        spdlog::error("Assimp error: {}", importer.GetErrorString());
        return nullptr;
    }

    auto model = std::make_unique<Model>();
    std::filesystem::path directory = path.parent_path();

    // 1. Process Materials
    std::vector<int32_t> materialIndices;
    if (scene->HasMaterials()) {
         for (unsigned int i = 0; i < scene->mNumMaterials; i++) {
             aiMaterial* aiMat = scene->mMaterials[i];
             Material material;
             aiString matName;
             aiMat->Get(AI_MATKEY_NAME, matName);
             material.name = matName.C_Str();
             
             // Initialize defaults
             material.gpuData.baseColorFactor = glm::vec4(1.0f);
             material.gpuData.metallicFactor = 0.1f;
             material.gpuData.roughnessFactor = 0.5f;
             material.gpuData.alphaCutoff = 0.5f;
             
             // Base Color / Diffuse
             aiColor4D color;
             if (AI_SUCCESS == aiMat->Get(AI_MATKEY_BASE_COLOR, color)) {
                 material.gpuData.baseColorFactor = glm::vec4(color.r, color.g, color.b, color.a);
             } else if (AI_SUCCESS == aiMat->Get(AI_MATKEY_COLOR_DIFFUSE, color)) {
                 material.gpuData.baseColorFactor = glm::vec4(color.r, color.g, color.b, color.a);
             }
 
             // Metallic / Roughness (PBR)
             float metallic = 0.0f;
             if (AI_SUCCESS == aiMat->Get(AI_MATKEY_METALLIC_FACTOR, metallic)) {
                 material.gpuData.metallicFactor = metallic;
             }
             
             float roughness = 0.5f;
             if (AI_SUCCESS == aiMat->Get(AI_MATKEY_ROUGHNESS_FACTOR, roughness)) {
                 material.gpuData.roughnessFactor = roughness;
             }
 
             // Textures
             // Helper to load texture
             auto loadAndRegister = [&](aiTextureType type, int32_t& targetIndex, std::shared_ptr<Image>& targetPtr) {
                 if (aiMat->GetTextureCount(type) > 0) {
                     aiString str;
                     aiMat->GetTexture(type, 0, &str);
                     std::filesystem::path texPath = directory / str.C_Str();
                     int texIdx = loadTexture(texPath, model.get(), assetManager, directory);
                     if (texIdx >= 0) {
                        targetIndex = model->textureIndices[texIdx];
                        // Get the image pointer we just added/found
                        if (texIdx < static_cast<int>(model->images.size())) {
                            targetPtr = model->images[texIdx];
                        }
                     } else {
                        targetIndex = -1;
                     }
                 } else {
                     targetIndex = -1;
                 }
             };
 
             loadAndRegister(aiTextureType_BASE_COLOR, material.gpuData.baseColorIndex, material.baseColorTexture);
             if (material.gpuData.baseColorIndex == -1) // Fallback to diffuse
                 loadAndRegister(aiTextureType_DIFFUSE, material.gpuData.baseColorIndex, material.baseColorTexture);
                 
             loadAndRegister(aiTextureType_NORMALS, material.gpuData.normalIndex, material.normalTexture);
             if (material.gpuData.normalIndex == -1)
                 loadAndRegister(aiTextureType_HEIGHT, material.gpuData.normalIndex, material.normalTexture);
             if (material.gpuData.normalIndex == -1)
                 loadAndRegister(aiTextureType_NORMAL_CAMERA, material.gpuData.normalIndex, material.normalTexture);
             loadAndRegister(aiTextureType_METALNESS, material.gpuData.metallicRoughnessIndex, material.metallicRoughnessTexture);
             if (material.gpuData.metallicRoughnessIndex == -1)
                 loadAndRegister(aiTextureType_SPECULAR, material.gpuData.metallicRoughnessIndex, material.metallicRoughnessTexture);
             if (material.gpuData.metallicRoughnessIndex == -1)
                 loadAndRegister(aiTextureType_SHININESS, material.gpuData.metallicRoughnessIndex, material.metallicRoughnessTexture);
             if (material.gpuData.metallicRoughnessIndex == -1)
                 loadAndRegister(aiTextureType_UNKNOWN, material.gpuData.metallicRoughnessIndex, material.metallicRoughnessTexture);
             if (material.gpuData.metallicRoughnessIndex == -1)
                 loadAndRegister(aiTextureType_DIFFUSE_ROUGHNESS, material.gpuData.metallicRoughnessIndex, material.metallicRoughnessTexture);
             
             loadAndRegister(aiTextureType_EMISSIVE, material.gpuData.emissiveIndex, material.emissiveTexture);
             loadAndRegister(aiTextureType_AMBIENT_OCCLUSION, material.gpuData.occlusionIndex, material.occlusionTexture);
 
             materialIndices.push_back(sceneManager->addMaterial(material));
         }
    }
    
    // Default material if none
    if (materialIndices.empty()) {
        Material defaultMat;
        defaultMat.name = "Default";
        materialIndices.push_back(sceneManager->addMaterial(defaultMat));
    }

    // 2. Process Meshes
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;

    if (scene->HasMeshes()) {
        for (unsigned int i = 0; i < scene->mNumMeshes; i++) {
            aiMesh* mesh = scene->mMeshes[i];
            Mesh meshData;
            meshData.name = mesh->mName.C_Str();
            
            Primitive primitive;
            primitive.firstIndex = static_cast<uint32_t>(indices.size());
            uint32_t vertexStart = static_cast<uint32_t>(vertices.size());
            
            // Bounds calculation
            glm::vec3 minPos(std::numeric_limits<float>::max());
            glm::vec3 maxPos(std::numeric_limits<float>::lowest());

            // Vertices
            for (unsigned int v = 0; v < mesh->mNumVertices; v++) {
                Vertex vertex{};
                
                // Position
                vertex.position = glm::vec3(mesh->mVertices[v].x, mesh->mVertices[v].y, mesh->mVertices[v].z);
                minPos = glm::min(minPos, vertex.position);
                maxPos = glm::max(maxPos, vertex.position);

                // Normal
                if (mesh->HasNormals()) {
                    vertex.normal = glm::vec3(mesh->mNormals[v].x, mesh->mNormals[v].y, mesh->mNormals[v].z);
                }

                // UV
                if (mesh->mTextureCoords[0]) {
                    vertex.uv = glm::vec2(mesh->mTextureCoords[0][v].x, mesh->mTextureCoords[0][v].y);
                } else {
                    vertex.uv = glm::vec2(0.0f, 0.0f);
                }

                // Tangent
                if (mesh->HasTangentsAndBitangents()) {
                    vertex.tangent = glm::vec4(mesh->mTangents[v].x, mesh->mTangents[v].y, mesh->mTangents[v].z, 1.0f);
                }
                
                // Color
                 if (mesh->HasVertexColors(0)) {
                    vertex.color = glm::vec4(mesh->mColors[0][v].r, mesh->mColors[0][v].g, mesh->mColors[0][v].b, mesh->mColors[0][v].a);
                 } else {
                     vertex.color = glm::vec4(1.0f);
                 }

                vertices.push_back(vertex);
            }

            // Indices
            for (unsigned int f = 0; f < mesh->mNumFaces; f++) {
                aiFace face = mesh->mFaces[f];
                for (unsigned int j = 0; j < face.mNumIndices; j++) {
                    indices.push_back(vertexStart + face.mIndices[j]);
                }
            }
            
            primitive.indexCount = static_cast<uint32_t>(indices.size()) - primitive.firstIndex;
            primitive.boundingCenter = (minPos + maxPos) * 0.5f;
            primitive.boundingRadius = glm::distance(maxPos, primitive.boundingCenter);
            primitive.materialIndex = materialIndices[mesh->mMaterialIndex];

            meshData.primitives.push_back(primitive);
            model->meshes.push_back(meshData);
        }
    }

    // Upload Buffers
    if (!vertices.empty()) {
        model->vertexBuffer = std::make_unique<Buffer>(
            m_context,
            vertices.size() * sizeof(Vertex),
            VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
            VMA_MEMORY_USAGE_CPU_TO_GPU
        );
        model->vertexBuffer->upload(vertices.data(), vertices.size() * sizeof(Vertex));
    }

    if (!indices.empty()) {
        model->indexBuffer = std::make_unique<Buffer>(
            m_context,
            indices.size() * sizeof(uint32_t),
            VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
            VMA_MEMORY_USAGE_CPU_TO_GPU
        );
        model->indexBuffer->upload(indices.data(), indices.size() * sizeof(uint32_t));
    }

    spdlog::info("Loaded model via Assimp: {} ({} meshes, {} materials)", path.string(), model->meshes.size(), materialIndices.size());
    return model;
}

int AssimpLoader::loadTexture(const std::filesystem::path& path, Model* model, AssetManager* assetManager, const std::filesystem::path& modelDir) {
    // Strategy for finding textures:
    // 1. Try absolute or relative path as given
    // 2. Try in the same directory as the model
    // 3. Try in a 'textures' subdirectory of the model's directory
    // 4. Try in a sibling 'textures' directory (e.g. model in 'source', textures in 'textures')
    // 5. Try in the parent directory
    // 6. Try recursive search in model's parent directory tree

    std::string filename = path.filename().string();
    
    std::vector<std::filesystem::path> pathsToTry;
    pathsToTry.push_back(path); // 1. As is
    
    // If path is relative, try combining with model dir
    if (path.is_relative()) {
         pathsToTry.push_back(modelDir / path);
    }

    pathsToTry.push_back(modelDir / filename); // 2. Same dir
    pathsToTry.push_back(modelDir / "textures" / filename); // 3. Subdirectory 'textures'
    pathsToTry.push_back(modelDir / ".." / "textures" / filename); // 4. Sibling 'textures'
    pathsToTry.push_back(modelDir / ".." / filename); // 5. Parent directory
    
    // 6. Additional search paths for nested structures
    // Try to find 'textures' folder anywhere in parent hierarchy
    std::filesystem::path searchRoot = modelDir.parent_path(); // Go up one level from source/
    pathsToTry.push_back(searchRoot / filename);
    pathsToTry.push_back(searchRoot / "textures" / filename);
    
    // 7. Try grandparent textures directory
    if (searchRoot.has_parent_path()) {
        std::filesystem::path grandparent = searchRoot.parent_path();
        pathsToTry.push_back(grandparent / "textures" / filename);
        pathsToTry.push_back(grandparent / filename);
    }

    std::shared_ptr<Image> image = nullptr;
    std::filesystem::path foundPath;

    spdlog::info("Looking for texture: {}", path.string());

    for (const auto& tryPath : pathsToTry) {
        try {
             // Clean up path (normalization)
             std::filesystem::path cleanPath = std::filesystem::weakly_canonical(tryPath);
             spdlog::debug("Checking path: {}", cleanPath.string());
             
             if (std::filesystem::exists(cleanPath)) {
                 image = assetManager->getOrLoadTexture(cleanPath);
                 if (image) {
                     foundPath = cleanPath;
                     spdlog::info("Found texture at: {}", cleanPath.string());
                     break; 
                 }
             }
        } catch (const std::exception& e) {
            spdlog::warn("Path check failed for '{}': {}", tryPath.string(), e.what());
        } catch (...) {
            spdlog::warn("Path check failed for '{}': Unknown error", tryPath.string());
        }
    }
    
    // 8. If still not found, try recursive search in model's base directory
    if (!image) {
        try {
            std::filesystem::path baseDir = modelDir.parent_path(); // e.g. m1897-trenchgun/
            if (std::filesystem::exists(baseDir) && std::filesystem::is_directory(baseDir)) {
                spdlog::debug("Searching recursively in: {}", baseDir.string());
                for (const auto& entry : std::filesystem::recursive_directory_iterator(baseDir)) {
                    if (entry.is_regular_file() && entry.path().filename() == filename) {
                        image = assetManager->getOrLoadTexture(entry.path());
                        if (image) {
                            foundPath = entry.path();
                            spdlog::info("Found texture via recursive search: {}", foundPath.string());
                            break;
                        }
                    }
                }
            }
        } catch (const std::exception& e) {
            spdlog::debug("Recursive search failed: {}", e.what());
        }
    }
    
    // 9. Fuzzy matching - try to find textures with similar suffixes
    // e.g., "Cartridge_low_Cartridge_BaseColor.png" -> look for "*BaseColor.png" or "*_BaseColor.png"
    if (!image) {
        try {
            // Extract texture type suffix from filename
            std::vector<std::string> textureSuffixes = {"BaseColor", "Diffuse", "Normal", "Metallic", "Roughness", "MetallicRoughness", "Occlusion", "Emissive", "Height"};
            std::string matchSuffix;
            for (const auto& suffix : textureSuffixes) {
                if (filename.find(suffix) != std::string::npos) {
                    matchSuffix = suffix;
                    break;
                }
            }
            
            if (!matchSuffix.empty()) {
                std::filesystem::path baseDir = modelDir.parent_path();
                std::filesystem::path texturesDir = baseDir / "textures";
                
                if (std::filesystem::exists(texturesDir) && std::filesystem::is_directory(texturesDir)) {
                    spdlog::debug("Fuzzy search for *{}.png in {}", matchSuffix, texturesDir.string());
                    
                    // Try to find a file with matching suffix and extension
                    std::string extension = path.extension().string();
                    for (const auto& entry : std::filesystem::directory_iterator(texturesDir)) {
                        if (!entry.is_regular_file()) continue;
                        
                        std::string entryName = entry.path().filename().string();
                        // Check if this file ends with the same suffix + extension
                        // e.g., looking for "BaseColor" in "Cartridge_BaseColor.png"
                        if (entryName.find(matchSuffix) != std::string::npos && 
                            entry.path().extension() == path.extension()) {
                            // Additional check: first meaningful word should match
                            // "Cartridge_low_Cartridge_BaseColor" should match "Cartridge_BaseColor"
                            std::string searchName = path.stem().string();
                            std::string candidateName = entry.path().stem().string();
                            
                            // Simple heuristic: check if candidate contains parts of search name
                            // Split by underscore and check for common parts
                            bool likelyMatch = false;
                            
                            // Find the first meaningful word (before _low_ or similar)
                            size_t pos = searchName.find("_low_");
                            if (pos == std::string::npos) pos = searchName.find("_Low_");
                            if (pos == std::string::npos) pos = searchName.find("_");
                            
                            std::string searchPrefix = (pos != std::string::npos) ? searchName.substr(0, pos) : searchName;
                            
                            if (!searchPrefix.empty() && candidateName.find(searchPrefix) != std::string::npos) {
                                likelyMatch = true;
                            }
                            
                            if (likelyMatch) {
                                image = assetManager->getOrLoadTexture(entry.path());
                                if (image) {
                                    foundPath = entry.path();
                                    spdlog::info("Found texture via fuzzy match: {} -> {}", filename, foundPath.string());
                                    break;
                                }
                            }
                        }
                    }
                }
            }
        } catch (const std::exception& e) {
            spdlog::debug("Fuzzy search failed: {}", e.what());
        }
    }

    if (!image) {
        spdlog::warn("Failed to find texture '{}'. Searched {} locations + recursive + fuzzy.", path.string(), pathsToTry.size());
        return -1;
    } 
    // else, image found, proceed
    
    try {
        // Register with descriptor manager
        SamplerSpecs specs;
        specs.magFilter = VK_FILTER_LINEAR;
        specs.minFilter = VK_FILTER_LINEAR;
        specs.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
        specs.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        specs.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        specs.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        specs.anisotropyEnable = true;
        specs.maxAnisotropy = 16.0f;
        
        VkSampler sampler = assetManager->getSampler(specs);

        uint32_t texIdx = m_context->getDescriptorManager().registerImage(image->getView(), sampler);
        
        model->images.push_back(image);
        model->textureIndices.push_back(texIdx);
        
        return static_cast<int>(model->textureIndices.size() - 1);
    } catch (const std::exception& e) {
        spdlog::error("Error registering texture: {}", e.what());
        return -1;
    }
}

} // namespace astral
