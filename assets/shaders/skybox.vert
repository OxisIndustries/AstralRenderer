#version 460
#extension GL_EXT_nonuniform_qualifier : enable

layout(location = 0) out vec3 outUVW;

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

layout(std430, set = 0, binding = 1) readonly buffer SceneDataBuffer {
    SceneData scene;
} allSceneBuffers[];

layout(push_constant) uniform PushConstants {
    uint sceneDataIndex;
    uint skyboxTextureIndex;
} pc;

void main() {
    SceneData scene = allSceneBuffers[pc.sceneDataIndex].scene;
    
    // Remove translation from view matrix
    mat4 view = mat4(mat3(scene.view));
    
    // Create a cube centered at the camera
    // Special full-screen cube trick or just a big cube
    // Here we use a standard cube approach
    // But for a better trick: use a full-screen quad and unproject
    
    vec3 vertices[8] = vec3[](
        vec3(-1, -1, -1), vec3(1, -1, -1), vec3(1, 1, -1), vec3(-1, 1, -1),
        vec3(-1, -1, 1), vec3(1, -1, 1), vec3(1, 1, 1), vec3(-1, 1, 1)
    );
    
    int indices[36] = int[](
        0, 1, 2, 2, 3, 0, 4, 5, 6, 6, 7, 4, // front, back
        1, 5, 6, 6, 2, 1, 0, 4, 7, 7, 3, 0, // right, left
        3, 2, 6, 6, 7, 3, 0, 1, 5, 5, 4, 0  // top, bottom
    );

    vec3 pos = vertices[indices[gl_VertexIndex]];
    outUVW = pos;
    
    // Set z to w so that depth is always 1.0
    vec4 clipPos = scene.proj * view * vec4(pos, 1.0);
    gl_Position = clipPos.xyww;
}
