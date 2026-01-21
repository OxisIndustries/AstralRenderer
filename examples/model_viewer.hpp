#pragma once

#include "astral/application.hpp"
#include <spdlog/spdlog.h>
#include <iostream>
#include <filesystem>

namespace astral {

class ModelViewer : public AstralApp {
public:
    ModelViewer() = default;
    virtual ~ModelViewer() override = default;

protected:
    virtual std::filesystem::path getModelPath() = 0;

    void initScene() override {
        // 1. Load HDR Environment
        std::filesystem::path hdrPath = "assets/textures/skybox.hdr";
        if (std::filesystem::exists(hdrPath)) {
            getEnvironmentManager()->loadHDR(hdrPath.string());
            spdlog::info("Loaded HDR environment: {}", hdrPath.string());
        } else {
            spdlog::warn("HDR environment not found at: {}", hdrPath.string());
        }

        // 2. Add standard lighting
        Light sun;
        sun.position = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f); // Directional
        sun.direction = glm::vec4(glm::normalize(glm::vec3(-0.5f, -1.0f, -0.5f)), 0.0f);
        sun.color = glm::vec4(1.0f, 1.0f, 1.0f, 4.0f); // Boosted intensity for better visibility
        getSceneManager()->addLight(sun);

        // 3. Load the model
        std::filesystem::path modelPath = getModelPath();
        spdlog::info("Loading model from: {}", std::filesystem::absolute(modelPath).string());

        try {
            m_model = getAssetManager()->loadModel(modelPath, getSceneManager());
            
            if (m_model) {
                spdlog::info("Model loaded successfully: {}", modelPath.string());
                
                // Calculate total bounding box
                glm::vec3 minBound(std::numeric_limits<float>::max());
                glm::vec3 maxBound(std::numeric_limits<float>::lowest());
                bool hasBounds = false;

                for (const auto& mesh : m_model->meshes) {
                    for (const auto& prim : mesh.primitives) {
                        glm::vec3 primMin = prim.boundingCenter - glm::vec3(prim.boundingRadius);
                        glm::vec3 primMax = prim.boundingCenter + glm::vec3(prim.boundingRadius);
                        minBound = glm::min(minBound, primMin);
                        maxBound = glm::max(maxBound, primMax);
                        hasBounds = true;
                    }
                }

                if (hasBounds) {
                    glm::vec3 size = maxBound - minBound;
                    spdlog::info("Model Bounds: Min({:.2f}, {:.2f}, {:.2f}), Max({:.2f}, {:.2f}, {:.2f})", 
                        minBound.x, minBound.y, minBound.z, maxBound.x, maxBound.y, maxBound.z);
                    spdlog::info("Model Size: ({:.2f}, {:.2f}, {:.2f})", size.x, size.y, size.z);
                    
                    // Center camera based on model size
                    float maxDim = std::max({size.x, size.y, size.z});
                    getCamera().setPosition(minBound + size * 0.5f + glm::vec3(0.0f, 0.0f, maxDim * 2.0f));
                } else {
                    spdlog::warn("Model has no primitives or bounds.");
                    getCamera().setPosition(glm::vec3(0.0f, 2.0f, 10.0f));
                }
                
                getCamera().setRotation(0.0f, -90.0f);
            } else {
                spdlog::error("Failed to load model: {}", modelPath.string());
            }
        } catch (const std::exception& e) {
            spdlog::error("Exception during model loading {}: {}", modelPath.string(), e.what());
            throw; 
        }
    }
};

} // namespace astral
