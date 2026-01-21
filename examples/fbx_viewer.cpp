#include "model_viewer.hpp"

class FbxViewer : public astral::ModelViewer {
protected:
    std::filesystem::path getModelPath() override {
        return "assets/models/m1897-trenchgun/source/trenchgun.fbx";
    }
};

int main() {
    try {
        FbxViewer app;
        app.run();
    } catch (const std::exception& e) {
        std::cerr << "Fatal Error: " << e.what() << std::endl;
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
