#pragma once

#include <vulkan/vulkan.h>
#include <glm/glm.hpp>
#include <vector>
#include <memory>
#include <string>
#include "astral/resources/buffer.hpp"
#include "astral/resources/image.hpp"

namespace astral {

struct Vertex {
    glm::vec3 position;
    glm::vec3 normal;
    glm::vec2 uv;
    glm::vec4 tangent;
    glm::vec4 color;

    static VkVertexInputBindingDescription getBindingDescription();
    static std::vector<VkVertexInputAttributeDescription> getAttributeDescriptions();
};

struct Primitive {
    uint32_t firstIndex;
    uint32_t indexCount;
    int32_t materialIndex; // SceneManager'daki material metadata buffer indeksi
    glm::vec3 boundingCenter;
    float boundingRadius;
};

struct Mesh {
    std::vector<Primitive> primitives;
    std::string name;
};

struct Model {
    std::vector<Mesh> meshes;
    std::unique_ptr<Buffer> vertexBuffer;
    std::unique_ptr<Buffer> indexBuffer;
    
    // Model içindeki tüm dokular (bindless sisteme kayıtlı)
    // Model içindeki tüm dokular (bindless sisteme kayıtlı)
    std::vector<std::shared_ptr<Image>> images;
    std::vector<uint32_t> textureIndices; 

    struct Node {
        Node* parent;
        std::vector<std::unique_ptr<Node>> children;
        int32_t meshIndex = -1;
        glm::mat4 matrix;
        std::string name;
    };

    std::vector<std::unique_ptr<Node>> nodes;
    std::vector<Node*> linearNodes;
};

} // namespace astral
