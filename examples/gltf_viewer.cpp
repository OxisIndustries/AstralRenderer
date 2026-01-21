#include "model_viewer.hpp"

class GltfViewer : public astral::ModelViewer {
protected:
    std::filesystem::path getModelPath() override {
        return "assets/models/boeing_ch-47_chinook_military_transport_aircraft/scene.gltf";
    }
};

int main() {
    try {
        GltfViewer app;
        app.run();
    } catch (const std::exception& e) {
        std::cerr << "Fatal Error: " << e.what() << std::endl;
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
