#include "astral/core/config.hpp"
#include <fstream>
#include <spdlog/spdlog.h>

namespace astral {

void Config::load(const std::string& path) {
    if (!std::filesystem::exists(path)) {
        spdlog::info("Config file {} not found, using defaults.", path);
        return;
    }

    try {
        std::ifstream file(path);
        file >> m_data;

        // Load General
        if (m_data.contains("general")) {
            auto& g = m_data["general"];
            general.windowWidth = g.value("windowWidth", 1600);
            general.windowHeight = g.value("windowHeight", 900);
            general.fullscreen = g.value("fullscreen", false);
            general.lastModelPath = g.value("lastModelPath", "");
        }

        spdlog::info("Config loaded from {}.", path);
    } catch (const std::exception& e) {
        spdlog::error("Failed to load config {}: {}", path, e.what());
    }
}

void Config::save(const std::string& path) {
    try {
        m_data["general"]["windowWidth"] = general.windowWidth;
        m_data["general"]["windowHeight"] = general.windowHeight;
        m_data["general"]["fullscreen"] = general.fullscreen;
        m_data["general"]["lastModelPath"] = general.lastModelPath;

        std::ofstream file(path);
        file << m_data.dump(4);
        spdlog::info("Config saved to {}.", path);
    } catch (const std::exception& e) {
        spdlog::error("Failed to save config {}: {}", path, e.what());
    }
}

void Config::applyTo(RendererSystem::UIParams& params) {
    if (!m_data.contains("renderer")) return;
    auto& r = m_data["renderer"];

    params.exposure = r.value("exposure", params.exposure);
    params.bloomStrength = r.value("bloomStrength", params.bloomStrength);
    params.gamma = r.value("gamma", params.gamma);
    params.iblIntensity = r.value("iblIntensity", params.iblIntensity);
    params.enableFXAA = r.value("enableFXAA", params.enableFXAA);
    params.enableSSAO = r.value("enableSSAO", params.enableSSAO);
    params.shadowBias = r.value("shadowBias", params.shadowBias);
    params.shadowNormalBias = r.value("shadowNormalBias", params.shadowNormalBias);
    params.pcfRange = r.value("pcfRange", params.pcfRange);
}

void Config::updateFrom(const RendererSystem::UIParams& params) {
    auto& r = m_data["renderer"];
    r["exposure"] = params.exposure;
    r["bloomStrength"] = params.bloomStrength;
    r["gamma"] = params.gamma;
    r["iblIntensity"] = params.iblIntensity;
    r["enableFXAA"] = params.enableFXAA;
    r["enableSSAO"] = params.enableSSAO;
    r["shadowBias"] = params.shadowBias;
    r["shadowNormalBias"] = params.shadowNormalBias;
    r["pcfRange"] = params.pcfRange;
}

} // namespace astral
