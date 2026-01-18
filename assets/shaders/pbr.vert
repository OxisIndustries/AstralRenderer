#version 460
#extension GL_EXT_nonuniform_qualifier : enable
#extension GL_EXT_buffer_reference : enable

layout(location = 0) in vec3 inPos;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inUV;
layout(location = 3) in vec4 inTangent;
layout(location = 4) in vec4 inColor;

layout(location = 0) out vec3 outWorldPos;
layout(location = 1) out vec3 outNormal;
layout(location = 2) out vec2 outUV;
layout(location = 3) out vec4 outColor;
layout(location = 4) out vec4 outTangent;
layout(location = 5) out flat uint outMaterialIndex;
layout(location = 6) out vec4 outCurClipPos;
layout(location = 7) out vec4 outPrevClipPos;

// Structures (keeping them to compile, but unused)
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

struct Material {
  vec4 baseColorFactor;
  vec4 emissiveFactor;
  float metallicFactor;
  float roughnessFactor;
  float alphaCutoff;
  uint alphaMode;
  
  float transmissionFactor;
  float ior;
  float thicknessFactor;
  uint doubleSided;
  
  int baseColorTextureIndex;
  int normalTextureIndex;
  int metallicRoughnessTextureIndex;
  int emissiveTextureIndex;
  
  int occlusionTextureIndex;
  int transmissionTextureIndex;
  int thicknessTextureIndex;
  uint padding;
};

layout(std430, set = 0, binding = 1) readonly buffer SceneBuffer {
  SceneData scene;
}
allSceneBuffers[];

layout(std430, set = 0, binding = 6) readonly buffer InstanceBuffer {
  MeshInstance instances[];
}
allInstanceBuffers[];

layout(std430, set = 0, binding = 2) readonly buffer MaterialBuffer {
  Material materials[];
}
allMaterialBuffers[];

layout(push_constant) uniform PushConstants {
  uint sceneDataIndex;
  uint instanceBufferIndex;
  uint materialBufferIndex;
}
pc;

// ... (buffer definitions)
void main() {
  SceneData scene = allSceneBuffers[pc.sceneDataIndex].scene;

  // STEP 2 RESTORATION: Use Instance Buffer
  MeshInstance instance =
      allInstanceBuffers[pc.instanceBufferIndex].instances[gl_InstanceIndex];
  mat4 modelMatrix = instance.transform;
  uint matIdx = instance.materialIndex;

  vec4 worldPos = modelMatrix * vec4(inPos, 1.0);
  outWorldPos = worldPos.xyz;

  mat3 normalMatrix = mat3(modelMatrix);
  outNormal = normalize(normalMatrix * inNormal);
  outTangent = vec4(normalize(normalMatrix * inTangent.xyz), inTangent.w);

  outUV = inUV;
  outColor = inColor;
  outMaterialIndex = matIdx; // instance.materialIndex;

  gl_Position = scene.viewProj * worldPos;

  // Velocity Calculation
  outCurClipPos = gl_Position;
  outPrevClipPos = scene.prevViewProj * worldPos;
}
