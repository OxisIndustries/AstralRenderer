#pragma once

#include "astral/core/context.hpp"
#include "astral/renderer/model.hpp"
#include "astral/renderer/scene_manager.hpp"
#include "astral/resources/sampler.hpp"
#include <filesystem>
#include <memory>
#include <string>
#include <unordered_map>
#include <functional>

namespace astral {

enum class TextureType {
    Albedo,
    Normal,
    MetallicRoughness,
    Occlusion,
    Emissive
};

class AssetManager;
class ModelLoader {
public:
    virtual ~ModelLoader() = default;
    virtual std::unique_ptr<Model> load(const std::filesystem::path& path, SceneManager* sceneManager, AssetManager* assetManager) = 0;
    virtual bool supportsExtension(const std::string& extension) const = 0;
};

class AssetManager {
public:
    AssetManager(Context* context);
    ~AssetManager() = default;

    void registerLoader(std::unique_ptr<ModelLoader> loader);
    
    // Loads a model from file, using the appropriate loader based on extension.
    // Uses caching to avoid reloading the same asset multiple times if requested (optional future improvement, currently direct load).
    // Loads a model from file, using the appropriate loader based on extension.
    // Uses caching to avoid reloading the same asset multiple times if requested (optional future improvement, currently direct load).
    std::unique_ptr<Model> loadModel(const std::filesystem::path& path, SceneManager* sceneManager);

    std::shared_ptr<Image> getOrLoadTexture(const std::filesystem::path& path, TextureType type = TextureType::Albedo);
    VkSampler getSampler(const SamplerSpecs& specs);

private:
    Context* m_context;
    std::vector<std::unique_ptr<ModelLoader>> m_loaders;
    std::unordered_map<std::string, std::shared_ptr<Image>> m_textureCache;
    std::unordered_map<SamplerSpecs, std::shared_ptr<Sampler>> m_samplerCache;
    std::shared_ptr<Image> m_errorTexture; // Magenta
    std::shared_ptr<Image> m_defaultNormalTexture; // Flat Blue
    std::shared_ptr<Image> m_whiteTexture; // White
    std::shared_ptr<Image> m_blackTexture; // Black
    
    // Future: Cache, Async loading queue
};

} // namespace astral
