#version 460
#extension GL_EXT_nonuniform_qualifier : enable

layout(location = 0) in vec3 inPos;
// layout(location = 1) in vec3 inNormal;
// layout(location = 2) in vec2 inUV;
// layout(location = 3) in vec4 inTangent;
// layout(location = 4) in vec4 inColor;

struct SceneData {
    mat4 view;
    mat4 proj;
    mat4 viewProj;
    mat4 invView;
    mat4 invProj;
    mat4 lightSpaceMatrix;
    mat4 cascadeViewProj[4];
    mat4 prevViewProj;
    vec4 frustumPlanes[6];
    vec4 cascadeSplits;
    vec4 cameraPos;
    vec2 jitter;
    int lightCount;
    int irradianceIndex;
    int prefilteredIndex;
    int brdfLutIndex;
    int shadowMapIndex;
    int lightBufferIndex;
    int headlampEnabled;
    int visualizeCascades;
    float shadowBias;
    float shadowNormalBias;
    int pcfRange;
    float csmLambda;
    int clusterBufferIndex;
    int clusterGridBufferIndex;
    int clusterLightIndexBufferIndex;
    int gridX, gridY, gridZ;
    float nearClip, farClip;
    float screenWidth, screenHeight;
    float iblIntensity;
    int sceneColorIndex;
    vec2 padding;
};

struct MeshInstance {
    mat4 transform;
    vec3 sphereCenter;
    float sphereRadius;
    uint materialIndex;
    uint padding[3];
};

// Bindless Set #0
layout(std430, set = 0, binding = 1) readonly buffer SceneDataBuffer {
    SceneData scene;
} allSceneBuffers[];

layout(std430, set = 0, binding = 6) readonly buffer InstanceBuffer {
    MeshInstance instances[];
} allInstanceBuffers[];

// Push Constants
layout(push_constant) uniform PushConstants {
    uint sceneDataIndex;
    uint instanceBufferIndex;
    uint materialBufferIndex;
    uint cascadeIndex; 
} pc;

void main() {
    SceneData scene = allSceneBuffers[pc.sceneDataIndex].scene;
    MeshInstance instance = allInstanceBuffers[pc.instanceBufferIndex].instances[gl_InstanceIndex];
    
    mat4 shadowMatrix = scene.lightSpaceMatrix;
    if (pc.cascadeIndex < 4) {
        shadowMatrix = scene.cascadeViewProj[pc.cascadeIndex];
    }
    
    gl_Position = shadowMatrix * instance.transform * vec4(inPos, 1.0);
}
