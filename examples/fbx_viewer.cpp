#include "astral/application.hpp"
#include <spdlog/spdlog.h>
#include <iostream>

class FbxViewerApp : public astral::AstralApp {
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
        // Using correct path as verified in planning phase
        std::string modelPath = "assets/models/bmw-e34-stance-style/source/sketchfab e34.fbx";
        m_model = getAssetManager()->loadModel(modelPath, getSceneManager());
        
        if (!m_model) {
            spdlog::error("Failed to load model: {}", modelPath);
        }

        // Adjust Camera for larger model if needed (typically cars are larger than helmet)
        // Adjusting based on assumption; verify visually
        getCamera().setPosition(glm::vec3(5.0f, 2.0f, 5.0f));
        getCamera().setRotation(-20.0f, -135.0f);

        // Default Lights
        astral::Light sun;
        sun.position = glm::vec4(10.0f, 10.0f, 10.0f, 1.0f); 
        sun.color = glm::vec4(1.0f, 0.95f, 0.9f, 8.0f);
        sun.direction = glm::vec4(0.0f, -1.0f, 0.0f, 20.0f);
        getSceneManager()->addLight(sun);

        astral::Light fillLight;
        fillLight.position = glm::vec4(-10.0f, 5.0f, -10.0f, 1.0f);
        fillLight.color = glm::vec4(0.8f, 0.8f, 1.0f, 3.0f);
        fillLight.direction = glm::vec4(0.0f, 0.0f, 0.0f, 15.0f);
        getSceneManager()->addLight(fillLight);
    }
};

int main() {
    try {
        FbxViewerApp app;
        app.run();
    } catch (const std::exception &e) {
        std::cerr << "Fatal Error: " << e.what() << std::endl;
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
