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
  float power;
} pc;

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
} allSceneBuffers[];
layout(std430, set = 0, binding = 13) readonly buffer KernelBuffers {
  vec4 samples[32];
} allKernelBuffers[];

layout(location = 0) in vec2 inUV;

// Reconstruct view-space position from depth buffer
vec3 getViewPos(vec2 uv, SceneData scene) {
  float depth = texture(textures[nonuniformEXT(pc.depthTextureIndex)], uv).r;
  if (depth >= 1.0)
    return vec3(0.0, 0.0, 1e6); // Infinite depth
  
  // Reconstruct position from depth
  // Note: OpenGL/Vulkan Clip Space: Z [-1, 1] or [0, 1].
  // AstralRenderer uses [0, 1] depth range (standard Vulkan).
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
  vec3 normal = normalize(texture(textures[nonuniformEXT(pc.normalTextureIndex)], inUV).rgb);

  // Get noise rotation
  ivec2 texSize = textureSize(textures[nonuniformEXT(pc.normalTextureIndex)], 0);
  ivec2 noiseSize = textureSize(textures[nonuniformEXT(pc.noiseTextureIndex)], 0);
  vec2 noiseUV = vec2(texSize) / vec2(noiseSize) * inUV;
  vec3 randomVec = normalize(texture(textures[nonuniformEXT(pc.noiseTextureIndex)], noiseUV).xyz);

  // Create TBN matrix (Gram-Schmidt process)
  vec3 tangent = normalize(randomVec - normal * dot(randomVec, normal));
  vec3 bitangent = cross(normal, tangent);
  mat3 TBN = mat3(tangent, bitangent, normal);

  float occlusion = 0.0;
  const int kernelSize = 32;
  
  for (int i = 0; i < kernelSize; ++i) {
    // get sample position
    vec3 samplePos = TBN * allKernelBuffers[nonuniformEXT(pc.kernelBufferIndex)].samples[i].xyz; 
    samplePos = fragPos + samplePos * pc.radius; 
    
    // project sample position (to texture coordinate)
    vec4 offset = vec4(samplePos, 1.0);
    offset = scene.proj * offset; // from view to clip-space
    offset.xyz /= offset.w; // perspective divide
    offset.xy = offset.xy * 0.5 + 0.5; // transform to [0,1] range

    // Skip if sample is outside screen
    if (offset.x < 0.0 || offset.x > 1.0 || offset.y < 0.0 || offset.y > 1.0) {
      continue;
    }
    
    // get sample depth
    float sampleDepth = getViewPos(offset.xy, scene).z;
    
    // range check & accumulate
    // smoothstep creates a smooth falloff based on radius
    float rangeCheck = smoothstep(0.0, 1.0, pc.radius / abs(fragPos.z - sampleDepth));
    
    // Checks if sample depth is closer to camera than sample position
    // samplePos.z is negative in View Space (Camera at 0, looking down -Z)
    // sampleDepth is also negative
    // If sampleDepth >= samplePos.z + bias, it means the geometry surface (sampleDepth) is CLOSER to camera (larger Z value, e.g. -5 > -10)
    // than the sample point. This means the sample point is OCCLUDED.
    // However, coordinate systems vary. 'getViewPos' usually returns -Z.
    // Let's assume standard Right Handed View Space: -Z forward.
    // If surface is at -5, sample point is at -5.1 (behind surface). 
    // -5 >= -5.1 + bias ? True. -> Occluded.
    // Wait. SSAO samples are in a hemisphere.
    // If I sample "into" the surface, samplePos.z might be -5.1. Surface is -5. 
    // Then -5 > -5.1. So it is occluded. Correct.
    occlusion += (sampleDepth >= samplePos.z + pc.bias ? 1.0 : 0.0) * rangeCheck;
  }
  
  occlusion = 1.0 - (occlusion / float(kernelSize));
  outSSAO = pow(occlusion, pc.power);
}
