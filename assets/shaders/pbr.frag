#version 460
#extension GL_EXT_nonuniform_qualifier : enable

layout(location = 0) in vec3 inWorldPos;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inUV;
layout(location = 3) in vec4 inColor;
layout(location = 4) in vec4 inTangent;
layout(location = 5) in flat uint inMaterialIndex;
layout(location = 6) in vec4 inCurClipPos;
layout(location = 7) in vec4 inPrevClipPos;

layout(location = 0) out vec4 outFragColor;
layout(location = 1) out vec4 outViewNormal;
layout(location = 2) out vec2 outVelocity;

struct Light {
  vec4 position;  // w = type
  vec4 color;     // w = intensity
  vec4 direction; // w = range
  vec4 params;    // x = inner, y = outer, z = shadowIndex
};

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
};

struct ClusterGrid {
  uint offset;
  uint count;
};

struct Material {
  vec4 baseColorFactor;
  float metallicFactor;
  float roughnessFactor;
  int baseColorTextureIndex;
  int metallicRoughnessTextureIndex;
  int normalTextureIndex;
  int occlusionTextureIndex;
  int emissiveTextureIndex;
  float alphaCutoff;
  int doubleSided;
  float padding1;
  float padding2;
  float padding3;
};

// Bindless Set #0
layout(set = 0, binding = 0) uniform sampler2D textures[];
layout(set = 0, binding = 12) uniform samplerCube skyboxes[];
layout(set = 0, binding = 1) readonly buffer SceneDataBuffer {
  SceneData scene;
}
allSceneBuffers[];
layout(set = 0, binding = 9) readonly buffer ClusterGridBuffer {
  ClusterGrid grids[];
}
allClusterGridBuffers[];
layout(set = 0, binding = 10) readonly buffer LightIndexBuffer {
  uint indices[];
}
allLightIndexBuffers[];
layout(set = 0, binding = 2) readonly buffer MaterialBuffer {
  Material materials[];
}
allMaterialBuffers[];
layout(set = 0, binding = 3) readonly buffer LightBuffer { Light lights[]; }
allLightBuffers[];
layout(set = 0, binding = 4) uniform sampler2DArray arrayTextures[];

layout(push_constant) uniform PushConstants {
  uint sceneDataIndex;
  uint instanceBufferIndex;
  uint materialBufferIndex;
  uint padding;
}
pc;

const float PI = 3.14159265359;

float DistributionGGX(vec3 N, vec3 H, float roughness) {
  float a = roughness * roughness;
  float a2 = a * a;
  float NdotH = max(dot(N, H), 0.0);
  float NdotH2 = NdotH * NdotH;

  float nom = a2;
  float denom = (NdotH2 * (a2 - 1.0) + 1.0);
  denom = PI * denom * denom;

  return nom / max(denom, 0.0000001); // Prevent division by zero
}

float GeometrySchlickGGX(float NdotV, float roughness) {
  float r = (roughness + 1.0);
  float k = (r * r) / 8.0;

  float nom = NdotV;
  float denom = NdotV * (1.0 - k) + k;

  return nom / denom;
}

float GeometrySmith(vec3 N, vec3 V, vec3 L, float roughness) {
  float NdotV = max(dot(N, V), 0.0);
  float NdotL = max(dot(N, L), 0.0);
  float ggx2 = GeometrySchlickGGX(NdotV, roughness);
  float ggx1 = GeometrySchlickGGX(NdotL, roughness);
  return ggx1 * ggx2;
}

vec3 fresnelSchlick(float cosTheta, vec3 F0) {
  return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

vec3 fresnelSchlickRoughness(float cosTheta, vec3 F0, float roughness) {
  return F0 + (max(vec3(1.0 - roughness), F0) - F0) *
                  pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

vec3 getNormalFromMap() {
  Material mat =
      allMaterialBuffers[pc.materialBufferIndex].materials[inMaterialIndex];
  if (mat.normalTextureIndex == -1) {
    if (mat.doubleSided == 1 && !gl_FrontFacing) {
        return normalize(-inNormal);
    }
    return normalize(inNormal);
  }

  vec3 tangentNormal =
      textureLod(textures[nonuniformEXT(mat.normalTextureIndex)], inUV, 0.0)
              .xyz *
          2.0 -
      1.0;

  vec3 N = normalize(inNormal);
  if (mat.doubleSided == 1 && !gl_FrontFacing) {
      N = -N;
  }
  vec3 T = normalize(inTangent.xyz);
  vec3 B = cross(N, T) * inTangent.w;
  mat3 TBN = mat3(T, B, N);

  return normalize(TBN * tangentNormal);
}

float calculateShadow(vec3 worldPos, vec3 normal, vec3 lightPos) {
  SceneData scene = allSceneBuffers[pc.sceneDataIndex].scene;
  if (scene.shadowMapIndex == -1)
    return 0.0;

  // Determine cascade based on view-space depth
  vec4 viewPos = scene.view * vec4(worldPos, 1.0);
  float depth = abs(viewPos.z);

  int cascadeIndex = 3;
  for (int i = 0; i < 4; ++i) {
    if (depth < scene.cascadeSplits[i]) {
      cascadeIndex = i;
      break;
    }
  }

  // Normal bias to further reduce acne
  vec3 offsetPos =
      worldPos + normal * scene.shadowNormalBias * (1.0 / (1.0 + cascadeIndex));
  vec4 lightSpacePos =
      scene.cascadeViewProj[cascadeIndex] * vec4(offsetPos, 1.0);

  // Perspective divide
  vec3 projCoords = lightSpacePos.xyz / lightSpacePos.w;

  // Transform to [0,1] range
  projCoords.xy = projCoords.xy * 0.5 + 0.5;

  if (projCoords.z > 1.0)
    return 0.0;

  float currentDepth = projCoords.z;

  // Bias to prevent shadow acne
  vec3 lightDir = normalize(lightPos - worldPos);
  float bias = max(scene.shadowBias * (1.0 - dot(normal, lightDir)),
                   scene.shadowBias * 0.1);
  // Adjust bias based on cascade (tighter bias for closer cascades)
  bias *= 1.0 / (1.0 + cascadeIndex);

  // PCF
  float shadow = 0.0;
  vec2 texelSize =
      1.0 /
      textureSize(arrayTextures[nonuniformEXT(scene.shadowMapIndex)], 0).xy;
  int range = scene.pcfRange;
  for (int x = -range; x <= range; ++x) {
    for (int y = -range; y <= range; ++y) {
      float pcfDepth =
          texture(arrayTextures[nonuniformEXT(scene.shadowMapIndex)],
                  vec3(projCoords.xy + vec2(x, y) * texelSize, cascadeIndex))
              .r;
      shadow += currentDepth - bias > pcfDepth ? 1.0 : 0.0;
    }
  }
  float samples = (range * 2 + 1) * (range * 2 + 1);
  shadow /= samples;

  return shadow;
}

void main() {
  // DEBUG CHECK REMOVED - RESTORING PBR LOGIC

  Material mat =
      allMaterialBuffers[pc.materialBufferIndex].materials[inMaterialIndex];
  SceneData scene = allSceneBuffers[pc.sceneDataIndex].scene;

  vec3 baseColor = mat.baseColorFactor.rgb;
  if (mat.baseColorTextureIndex != -1) {
    baseColor *= textureLod(textures[nonuniformEXT(mat.baseColorTextureIndex)],
                            inUV, 0.0)
                     .rgb;
  }

  float metallic = mat.metallicFactor;
  float roughness = mat.roughnessFactor;
  if (mat.metallicRoughnessTextureIndex != -1) {
    vec4 mrSample = textureLod(
        textures[nonuniformEXT(mat.metallicRoughnessTextureIndex)], inUV, 0.0);
    metallic *= mrSample.b;
    roughness *= mrSample.g;
  }

  vec3 N = getNormalFromMap();
  vec3 V = normalize(scene.cameraPos.xyz - inWorldPos);
  vec3 R = reflect(-V, N);

  vec3 F0 = vec3(0.04);
  F0 = mix(F0, baseColor, metallic);

  vec3 lo = vec3(0.0);

  // Clustered Lighting
  vec4 viewPos = scene.view * vec4(inWorldPos, 1.0);
  float zDepth = abs(viewPos.z);

  // Logarithmic slicing with safety check
  uint zSlice = 0;
  if (zDepth > scene.nearClip) {
    zSlice = uint(log(zDepth / scene.nearClip) * float(scene.gridZ) /
                  log(scene.farClip / scene.nearClip));
  }
  zSlice = min(zSlice, uint(scene.gridZ - 1));

  // Proper screen to cluster grid mapping
  uint xSlice = uint(gl_FragCoord.x / (scene.screenWidth / float(scene.gridX)));
  uint ySlice =
      uint(gl_FragCoord.y / (scene.screenHeight / float(scene.gridY)));

  xSlice = min(xSlice, uint(scene.gridX - 1));
  ySlice = min(ySlice, uint(scene.gridY - 1));

  uint clusterIdx =
      xSlice + (ySlice * scene.gridX) + (zSlice * scene.gridX * scene.gridY);
  clusterIdx =
      min(clusterIdx, uint(scene.gridX * scene.gridY * scene.gridZ - 1));

  ClusterGrid grid =
      allClusterGridBuffers[nonuniformEXT(scene.clusterGridBufferIndex)]
          .grids[clusterIdx];

  for (uint i = 0; i < grid.count; i++) {
    uint lightIdx =
        allLightIndexBuffers[nonuniformEXT(scene.clusterLightIndexBufferIndex)]
            .indices[grid.offset + i];
    Light light = allLightBuffers[scene.lightBufferIndex].lights[lightIdx];

    vec3 L;
    float attenuation = 1.0;

    if (light.position.w == 1.0) { // Directional
      L = normalize(-light.direction.xyz);
    } else { // Point
      L = normalize(light.position.xyz - inWorldPos);
      float distance = length(light.position.xyz - inWorldPos);
      attenuation = 1.0 / (distance * distance + 0.01);

      // Range attenuation
      if (light.direction.w > 0.0) {
        attenuation *= clamp(1.0 - (distance / light.direction.w), 0.0, 1.0);
      }
    }

    vec3 H = normalize(V + L);
    vec3 radiance = light.color.rgb * light.color.a * attenuation;

    // Cook-Torrance BRDF
    float D = DistributionGGX(N, H, roughness);
    float G = GeometrySmith(N, V, L, roughness);
    vec3 F = fresnelSchlick(max(dot(H, V), 0.0), F0);

    vec3 kS = F;
    vec3 kD = vec3(1.0) - kS;
    kD *= 1.0 - metallic;

    float NdotV_l = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    vec3 numerator = D * G * F;
    float denominator = 4.0 * NdotV_l * NdotL + 0.0001;
    vec3 specular = numerator / denominator;

    float shadow = 0.0;
    if (lightIdx == 0) { // Only first light casts shadow for now
      shadow = calculateShadow(inWorldPos, N, light.position.xyz);
    }

    lo += (kD * baseColor / PI + specular) * radiance * NdotL * (1.0 - shadow);
  }

  // Headlamp (always from camera)
  if (scene.headlampEnabled == 1) {
    vec3 headlampL = normalize(scene.cameraPos.xyz - inWorldPos);
    vec3 headlampH = normalize(V + headlampL);
    float headlampDistance = length(scene.cameraPos.xyz - inWorldPos);
    float headlampAttenuation =
        1.0 / (headlampDistance * headlampDistance + 0.01);
    vec3 headlampRadiance =
        vec3(1.5) * headlampAttenuation; // Slightly reduced intensity

    float D_h = DistributionGGX(N, headlampH, roughness);
    float G_h = GeometrySmith(N, V, headlampL, roughness);
    vec3 F_h = fresnelSchlick(max(dot(headlampH, V), 0.0), F0);

    vec3 kS_h = F_h;
    vec3 kD_h = (vec3(1.0) - kS_h) * (1.0 - metallic);

    vec3 specular_h =
        (D_h * G_h * F_h) /
        (4.0 * max(dot(N, V), 0.0) * max(dot(N, headlampL), 0.0) + 0.0001);
    lo += (kD_h * baseColor / PI + specular_h) * headlampRadiance *
          max(dot(N, headlampL), 0.0);
  }

  // Ambient / IBL
  vec3 ambient = vec3(0.03) * baseColor;
  if (scene.irradianceIndex != -1) {
    vec3 F_ibl = fresnelSchlickRoughness(max(dot(N, V), 0.0), F0, roughness);
    vec3 kS_ibl = F_ibl;
    vec3 kD_ibl = (vec3(1.0) - kS_ibl) * (1.0 - metallic);

    vec3 irradiance =
        texture(skyboxes[nonuniformEXT(scene.irradianceIndex)], N).rgb;
    vec3 diffuse = irradiance * baseColor;

    const float MAX_REFLECTION_LOD = 4.0;
    vec3 prefilteredColor =
        textureLod(skyboxes[nonuniformEXT(scene.prefilteredIndex)], R,
                   roughness * MAX_REFLECTION_LOD)
            .rgb;
    vec2 brdf = texture(textures[nonuniformEXT(scene.brdfLutIndex)],
                        vec2(max(dot(N, V), 0.0), roughness))
                    .rg;
    vec3 specular = prefilteredColor * (F_ibl * brdf.x + brdf.y);

    ambient = (kD_ibl * diffuse + specular) * scene.iblIntensity;
  }

  if (mat.occlusionTextureIndex != -1) {
    ambient *= textureLod(textures[nonuniformEXT(mat.occlusionTextureIndex)],
                          inUV, 0.0)
                   .r;
  }

  vec3 emissive = vec3(0.0);
  if (mat.emissiveTextureIndex != -1) {
    emissive =
        textureLod(textures[nonuniformEXT(mat.emissiveTextureIndex)], inUV, 0.0)
            .rgb;
  }

  // HDR and Tonemapping
  vec3 color = ambient + lo + emissive;

  // Visualize Cascades
  if (scene.visualizeCascades == 1) {
    vec4 viewPos = scene.view * vec4(inWorldPos, 1.0);
    float depth = abs(viewPos.z);
    vec3 cascadeColors[4] = vec3[4](vec3(1.0, 0.0, 0.0), // Red
                                    vec3(0.0, 1.0, 0.0), // Green
                                    vec3(0.0, 0.0, 1.0), // Blue
                                    vec3(1.0, 1.0, 0.0)  // Yellow
    );

    int cascadeIndex = 3;
    for (int i = 0; i < 4; ++i) {
      if (depth < scene.cascadeSplits[i]) {
        cascadeIndex = i;
        break;
      }
    }
    color *= cascadeColors[cascadeIndex];
  }

  outFragColor = vec4(color, 1.0);

  // Output view-space normals for SSAO
  vec3 viewNormal = mat3(scene.view) * N;
  outViewNormal = vec4(normalize(viewNormal), 1.0);

  // Velocity Buffer (Motion Vectors)
  vec2 cur = (inCurClipPos.xy / inCurClipPos.w) * 0.5 + 0.5;
  vec2 prev = (inPrevClipPos.xy / inPrevClipPos.w) * 0.5 + 0.5;

  // Remove jitter from current position
  cur -= scene.jitter;

  outVelocity = cur - prev;
}
