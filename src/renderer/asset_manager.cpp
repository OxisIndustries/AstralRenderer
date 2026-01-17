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
    
    // 1. Error Color (Magenta)
    m_errorTexture = std::make_shared<Image>(m_context, specs);
    uint8_t magenta[] = {255, 0, 255, 255};
    m_errorTexture->upload(magenta, 4);

    // 2. Default Normal (Flat Blue: 128, 128, 255)
    // Note: Normals shouldn't be SRGB, but our system currently expects SRGB for most. 
    // Actually, Normals SHOULD be UNORM. Let's create a UNORM spec for it.
    ImageSpecs normalSpecs = specs;
    normalSpecs.format = VK_FORMAT_R8G8B8A8_UNORM;
    m_defaultNormalTexture = std::make_shared<Image>(m_context, normalSpecs);
    uint8_t flatNormal[] = {128, 128, 255, 255};
    m_defaultNormalTexture->upload(flatNormal, 4);

    // 3. White
    m_whiteTexture = std::make_shared<Image>(m_context, specs);
    uint8_t white[] = {255, 255, 255, 255};
    m_whiteTexture->upload(white, 4);

    // 4. Black
    m_blackTexture = std::make_shared<Image>(m_context, specs);
    uint8_t black[] = {0, 0, 0, 255};
    m_blackTexture->upload(black, 4);
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

std::shared_ptr<Image> AssetManager::getOrLoadTexture(const std::filesystem::path& path, TextureType type) {
    auto getFallback = [&]() -> std::shared_ptr<Image> {
        switch (type) {
            case TextureType::Normal: return m_defaultNormalTexture;
            case TextureType::MetallicRoughness:
            case TextureType::Occlusion: return m_whiteTexture;
            case TextureType::Emissive: return m_blackTexture;
            case TextureType::Albedo:
            default: return m_errorTexture;
        }
    };

    if (!std::filesystem::exists(path)) {
        spdlog::warn("Texture file not found: {}, returning default for type", path.string());
        return getFallback();
    }

    std::string pathStr = std::filesystem::absolute(path).string();
    
    // Check cache
    if (m_textureCache.find(pathStr) != m_textureCache.end()) {
        return m_textureCache[pathStr];
    }

    spdlog::info("Loading texture: {}", pathStr);

    int width, height, channels;
    stbi_uc* pixels = stbi_load(pathStr.c_str(), &width, &height, &channels, STBI_rgb_alpha);

    if (!pixels) {
        spdlog::error("Failed to load texture image: {}", pathStr);
        return getFallback();
    }

    ImageSpecs specs;
    specs.width = static_cast<uint32_t>(width);
    specs.height = static_cast<uint32_t>(height);
    // Normals should be UNORM, others mostly SRGB.
    specs.format = (type == TextureType::Normal) ? VK_FORMAT_R8G8B8A8_UNORM : VK_FORMAT_R8G8B8A8_SRGB;
    specs.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;

    auto image = std::make_shared<Image>(m_context, specs);
    image->upload(pixels, specs.width * specs.height * 4);
    
    stbi_image_free(pixels);

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
