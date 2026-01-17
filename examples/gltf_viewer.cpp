#include "astral/application.hpp"
#include <spdlog/spdlog.h>
#include <iostream>

class GltfViewerApp : public astral::AstralApp {
protected:
    void initScene() override {
        // Load Skybox
        std::string hdrPath = "assets/textures/skybox.hdr";
        if (std::filesystem::exists(hdrPath)) {
            getEnvironmentManager()->loadHDR(hdrPath);
        } else {
            spdlog::warn("Skybox HDR not found at: {}. IBL will be disabled.", hdrPath);
        }

        astral::Material defaultMat;
    defaultMat.name = "Default";
    defaultMat.gpuData.baseColorFactor = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f);
    defaultMat.gpuData.metallicFactor = 0.5f;
    defaultMat.gpuData.roughnessFactor = 0.5f;
    getSceneManager()->addMaterial(defaultMat);

        // Load Model
        std::string modelPath = "assets/models/damaged_helmet/scene.gltf";
        m_model = getAssetManager()->loadModel(modelPath, getSceneManager());
        
        if (!m_model) {
            spdlog::error("Failed to load model: {}", modelPath);
        }

        // Default Lights
        astral::Light sun;
        sun.position = glm::vec4(5.0f, 8.0f, 5.0f, 1.0f); // Point light
        sun.color = glm::vec4(1.0f, 1.0f, 1.0f, 10.0f);
        sun.direction = glm::vec4(0.0f, -1.0f, 0.0f, 20.0f);
        sun.params = glm::vec4(0.0f, 0.0f, 0.0f, 0.0f);
        getSceneManager()->addLight(sun);

        astral::Light blueLight;
        blueLight.position = glm::vec4(-5.0f, 2.0f, -5.0f, 0.0f);
        blueLight.color = glm::vec4(0.2f, 0.4f, 1.0f, 5.0f);
        blueLight.direction = glm::vec4(0.0f, 0.0f, 0.0f, 15.0f);
        getSceneManager()->addLight(blueLight);
    }
};

int main() {
    try {
        GltfViewerApp app;
        app.run();
    } catch (const std::exception &e) {
        std::cerr << "Fatal Error: " << e.what() << std::endl;
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
