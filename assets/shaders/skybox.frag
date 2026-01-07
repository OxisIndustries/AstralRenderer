#version 460
#extension GL_EXT_nonuniform_qualifier : enable

layout(location = 0) in vec3 inUVW;
layout(location = 0) out vec4 outColor;
layout(location = 1) out vec4 outNormal;
layout(location = 2) out vec2 outVelocity;

layout(set = 0, binding = 12) uniform samplerCube skyboxes[];

layout(push_constant) uniform PushConstants {
    uint sceneDataIndex;
    uint skyboxTextureIndex;
} pc;

void main() {
    // Skybox texture index should be passed via push constants
    // For now, we assume it's a cube map registered in the bindless set
    vec3 envColor = texture(skyboxes[nonuniformEXT(pc.skyboxTextureIndex)], inUVW).rgb;
    
    outColor = vec4(envColor, 1.0);
    outNormal = vec4(0.0, 0.0, 0.0, 1.0); // No normal for skybox
    outVelocity = vec2(0.0); // Static skybox has no velocity
}
