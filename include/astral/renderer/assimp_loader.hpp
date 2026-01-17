#pragma once

#include "astral/renderer/model.hpp"
#include "astral/renderer/asset_manager.hpp"
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <filesystem>
#include <vector>

namespace astral {

class Context;
class SceneManager;

class AssimpLoader : public ModelLoader {
public:
    explicit AssimpLoader(Context* context);
    ~AssimpLoader() override;

    std::unique_ptr<Model> load(const std::filesystem::path& path, SceneManager* sceneManager, AssetManager* assetManager) override;
    bool supportsExtension(const std::string& extension) const override;

private:
    Context* m_context;

    int loadTexture(const std::filesystem::path& path, Model* model, AssetManager* assetManager);
};

} // namespace astral
