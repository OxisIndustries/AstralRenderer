#include "astral/renderer/asset_manager.hpp"
#include "astral/renderer/asset_manager.hpp"
#include "astral/resources/image.hpp"
#include <spdlog/spdlog.h>
#include <stb_image.h>
#include <algorithm>

namespace astral {

AssetManager::AssetManager(Context* context) : m_context(context) {
    ImageSpecs specs;
    specs.width = 1;
    specs.height = 1;
    specs.format = VK_FORMAT_R8G8B8A8_SRGB;
    specs.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    
    m_errorTexture = std::make_shared<Image>(m_context, specs);
    uint8_t pixels[] = {255, 0, 255, 255};
    m_errorTexture->upload(pixels, 4);
}

void AssetManager::registerLoader(std::unique_ptr<ModelLoader> loader) {
    m_loaders.push_back(std::move(loader));
}

std::unique_ptr<Model> AssetManager::loadModel(const std::filesystem::path& path, SceneManager* sceneManager) {
    if (!std::filesystem::exists(path)) {
        spdlog::error("Asset not found: {}", path.string());
        return nullptr;
    }

    std::string ext = path.extension().string();
    // Convert to lowercase for comparison
    std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c){ return std::tolower(c); });

    for (const auto& loader : m_loaders) {
        if (loader->supportsExtension(ext)) {
            spdlog::info("Loading asset: {} using appropriate loader...", path.string());
            return loader->load(path, sceneManager, this);
        }
    }

    spdlog::error("No loader found for extension: {}", ext);
    return nullptr;
    spdlog::error("No loader found for extension: {}", ext);
    return nullptr;
}

std::shared_ptr<Image> AssetManager::getOrLoadTexture(const std::filesystem::path& path) {
    if (!std::filesystem::exists(path)) {
        spdlog::warn("Texture file not found: {}", path.string());
        return m_errorTexture;
    }

    std::string pathStr = std::filesystem::absolute(path).string();
    
    // Check cache
    if (m_textureCache.find(pathStr) != m_textureCache.end()) {
        spdlog::debug("Texture cache hit: {}", pathStr);
        return m_textureCache[pathStr];
    }

    spdlog::info("Loading texture: {}", pathStr);

    int width, height, channels;
    stbi_uc* pixels = stbi_load(pathStr.c_str(), &width, &height, &channels, STBI_rgb_alpha);

    if (!pixels) {
        spdlog::error("Failed to load texture image: {}", pathStr);
        return m_errorTexture;
    }

    ImageSpecs specs;
    specs.width = static_cast<uint32_t>(width);
    specs.height = static_cast<uint32_t>(height);
    specs.format = VK_FORMAT_R8G8B8A8_SRGB;
    specs.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT; // TRANSFER_SRC_BIT added by Image ctor

    auto image = std::make_shared<Image>(m_context, specs);
    image->upload(pixels, specs.width * specs.height * 4);
    
    stbi_image_free(pixels);

    // Cache it
    m_textureCache[pathStr] = image;
    return image;
}

VkSampler AssetManager::getSampler(const SamplerSpecs& specs) {
    if (m_samplerCache.find(specs) != m_samplerCache.end()) {
        return m_samplerCache[specs]->getHandle();
    }
    
    auto sampler = std::make_shared<Sampler>(m_context, specs);
    m_samplerCache[specs] = sampler;
    return sampler->getHandle();
}

} // namespace astral
