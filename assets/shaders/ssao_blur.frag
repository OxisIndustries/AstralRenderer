#version 460
#extension GL_EXT_nonuniform_qualifier : enable

layout(location = 0) out float outColor;

layout(push_constant) uniform PushConstants {
    int inputTextureIndex;
} pc;

layout(set = 0, binding = 0) uniform sampler2D textures[];

layout(location = 0) in vec2 inUV;

void main() {
    vec2 texelSize = 1.0 / vec2(textureSize(textures[nonuniformEXT(pc.inputTextureIndex)], 0));
    float result = 0.0;
    for (int x = -2; x < 2; ++x) {
        for (int y = -2; y < 2; ++y) {
            vec2 offset = vec2(float(x), float(y)) * texelSize;
            result += texture(textures[nonuniformEXT(pc.inputTextureIndex)], inUV + offset).r;
        }
    }
    outColor = result / 16.0;
}
