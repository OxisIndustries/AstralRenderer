#version 460
#extension GL_EXT_nonuniform_qualifier : enable

layout(location = 0) out float outSSAO;

layout(push_constant) uniform PushConstants {
  int normalTextureIndex;
  int depthTextureIndex;
  int noiseTextureIndex;
  int kernelBufferIndex;
  float radius;
  float bias;
}
pc;

struct SceneData {
  mat4 view;
  mat4 proj;
  mat4 viewProj;
  mat4 invView;
  mat4 invProj;
  mat4 lightSpaceMatrix;
  mat4 cascadeViewProj[4];
  vec4 frustumPlanes[6];
  vec4 cascadeSplits;
  vec4 cameraPos;
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
  int padding1;
};

// Bindless Set #0
layout(set = 0, binding = 0) uniform sampler2D textures[];
layout(std430, set = 0, binding = 1) readonly buffer GlobalBuffers {
  SceneData scene;
}
allSceneBuffers[];
layout(std430, set = 0, binding = 13) readonly buffer KernelBuffers {
  vec4 samples[32];
}
allKernelBuffers[];

layout(location = 0) in vec2 inUV;

vec3 getViewPos(vec2 uv, SceneData scene) {
  float depth = texture(textures[nonuniformEXT(pc.depthTextureIndex)], uv).r;
  if (depth >= 1.0)
    return vec3(0.0, 0.0, 1e6); // Very far away
  vec4 clipPos = vec4(uv.x * 2.0 - 1.0, uv.y * 2.0 - 1.0, depth, 1.0);
  vec4 viewPos = scene.invProj * clipPos;
  return viewPos.xyz / viewPos.w;
}

void main() {
  SceneData scene = allSceneBuffers[0].scene;

  float depth = texture(textures[nonuniformEXT(pc.depthTextureIndex)], inUV).r;
  if (depth >= 1.0) {
    outSSAO = 1.0;
    return;
  }

  vec3 fragPos = getViewPos(inUV, scene);
  vec3 normal = normalize(
      texture(textures[nonuniformEXT(pc.normalTextureIndex)], inUV).rgb);

  // Get noise rotation
  ivec2 texSize =
      textureSize(textures[nonuniformEXT(pc.normalTextureIndex)], 0);
  ivec2 noiseSize =
      textureSize(textures[nonuniformEXT(pc.noiseTextureIndex)], 0);
  vec2 noiseUV = vec2(texSize) / vec2(noiseSize) * inUV;
  vec3 randomVec = normalize(
      texture(textures[nonuniformEXT(pc.noiseTextureIndex)], noiseUV).xyz);

  // Create TBN matrix
  vec3 tangent = normalize(randomVec - normal * dot(randomVec, normal));
  vec3 bitangent = cross(normal, tangent);
  mat3 TBN = mat3(tangent, bitangent, normal);

  float occlusion = 0.0;
  const int kernelSize = 32;
  for (int i = 0; i < kernelSize; ++i) {
    // Sample position in view space
    vec3 samplePos =
        TBN *
        allKernelBuffers[nonuniformEXT(pc.kernelBufferIndex)].samples[i].xyz;
    samplePos = fragPos + samplePos * pc.radius;

    // Project sample position
    vec4 offset = vec4(samplePos, 1.0);
    offset = scene.proj * offset;
    offset.xyz /= offset.w;
    offset.xy = offset.xy * 0.5 + 0.5;

    // Skip if sample is outside screen
    if (offset.x < 0.0 || offset.x > 1.0 || offset.y < 0.0 || offset.y > 1.0) {
      continue;
    }

    float sampleDepth = getViewPos(offset.xy, scene).z;

    float rangeCheck =
        smoothstep(0.0, 1.0, pc.radius / abs(fragPos.z - sampleDepth));
    occlusion +=
        (sampleDepth >= samplePos.z + pc.bias ? 1.0 : 0.0) * rangeCheck;
  }

  outSSAO = 1.0 - (occlusion / float(kernelSize));
}
