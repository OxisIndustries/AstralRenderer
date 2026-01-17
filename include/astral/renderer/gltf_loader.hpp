#pragma once

#include "astral/renderer/model.hpp"
#include "astral/renderer/asset_manager.hpp"
#include <filesystem>

namespace astral {

class Context;
class SceneManager;

class GltfLoader : public ModelLoader {
public:
    explicit GltfLoader(Context* context);
    ~GltfLoader() override;

    std::unique_ptr<Model> load(const std::filesystem::path& path, SceneManager* sceneManager, AssetManager* assetManager) override;
    bool supportsExtension(const std::string& extension) const override;

private:
    Context* m_context;

};

} // namespace astral
