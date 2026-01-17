#pragma once

#include <string>
#include <nlohmann/json.hpp>
#include "astral/renderer/renderer_system.hpp"

namespace astral {

class Config {
public:
    static Config& get() {
        static Config instance;
        return instance;
    }

    void load(const std::string& path = "config.json");
    void save(const std::string& path = "config.json");

    // Helper to sync with UIParams
    void applyTo(RendererSystem::UIParams& params);
    void updateFrom(const RendererSystem::UIParams& params);

    // Individual settings if needed
    struct {
        int windowWidth = 1600;
        int windowHeight = 900;
        bool fullscreen = false;
        std::string lastModelPath = "";
    } general;

private:
    Config() = default;
    
    // Internal helper to handle JSON serialization
    nlohmann::json m_data;
};

} // namespace astral
