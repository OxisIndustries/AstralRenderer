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
  int sceneColorIndex;
  vec2 padding;
};

struct ClusterGrid {
  uint offset;
  uint count;
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

const vec2 poissonDisk[16] = vec2[](
   vec2( -0.94201624, -0.39906216 ),
   vec2( 0.94558609, -0.76890725 ),
   vec2( -0.094184101, -0.92938870 ),
   vec2( 0.34495938, 0.29387760 ),
   vec2( -0.91588581, 0.45771432 ),
   vec2( -0.81544232, -0.87169214 ),
   vec2( 0.91046627, 0.72447952 ),
   vec2( 0.21045446, -0.82531383 ),
   vec2( 0.12543931, 0.22333204 ),
   vec2( 0.40100315, 0.82544100 ),
   vec2( -0.16132043, 0.73030273 ),
   vec2( -0.53305847, 0.28113706 ),
   vec2( 0.33842401, -0.61365885 ),
   vec2( -0.67615139, -0.44620702 ),
   vec2( 0.59793335, -0.51231192 ),
   vec2( 0.73946448, -0.18511059 )
);

float InterleavedGradientNoise(vec2 fragCoord) {
    vec3 magic = vec3(0.06711056f, 0.00583715f, 52.9829189f);
    return fract(magic.z * fract(dot(fragCoord, magic.xy)));
}

float sampleShadow(vec3 worldPos, vec3 normal, vec3 lightDir, int cascadeIndex, float nDotL, float angle) {
  SceneData scene = allSceneBuffers[pc.sceneDataIndex].scene;
  
  float normalBiasScale = clamp(1.0 - nDotL, 0.0, 1.0);
  vec3 offsetPos = worldPos + normal * scene.shadowNormalBias * normalBiasScale * (1.0 / (1.0 + float(cascadeIndex)));
  
  vec4 lightSpacePos = scene.cascadeViewProj[cascadeIndex] * vec4(offsetPos, 1.0);
  vec3 projCoords = lightSpacePos.xyz / lightSpacePos.w;
  projCoords.xy = projCoords.xy * 0.5 + 0.5;

  if (projCoords.z > 1.0) return 0.0;
  
  float currentDepth = projCoords.z;
  float bias = max(scene.shadowBias * (1.0 - nDotL), scene.shadowBias * 0.1);
  bias *= 1.0 / (1.0 + float(cascadeIndex));

  float shadow = 0.0;
  vec2 texelSize = 1.0 / textureSize(arrayTextures[nonuniformEXT(scene.shadowMapIndex)], 0).xy;
  
  float s = sin(angle);
  float c = cos(angle);
  mat2 rotationMatrix = mat2(c, -s, s, c);
  float spread = float(scene.pcfRange);

  for (int i = 0; i < 16; i++) {
    vec2 offset = (rotationMatrix * poissonDisk[i]) * texelSize * spread;
    float pcfDepth = texture(arrayTextures[nonuniformEXT(scene.shadowMapIndex)], 
                             vec3(projCoords.xy + offset, cascadeIndex)).r;
    shadow += currentDepth - bias > pcfDepth ? 1.0 : 0.0;
  }
  return shadow / 16.0;
}

float calculateShadow(vec3 worldPos, vec3 normal, vec3 lightPos) {
  SceneData scene = allSceneBuffers[pc.sceneDataIndex].scene;
  if (scene.shadowMapIndex == -1)
    return 0.0;

  vec3 lightDir = normalize(lightPos - worldPos);
  float nDotL = dot(normal, lightDir);

  vec4 viewPos = scene.view * vec4(worldPos, 1.0);
  float depth = abs(viewPos.z);

  int cascadeIndex = 3;
  for (int i = 0; i < 4; ++i) {
    if (depth < scene.cascadeSplits[i]) {
      cascadeIndex = i;
      break;
    }
  }

  float angle = InterleavedGradientNoise(gl_FragCoord.xy) * 2.0 * PI;
  float shadow = sampleShadow(worldPos, normal, lightDir, cascadeIndex, nDotL, angle);

  // Transition smoothing
  if (cascadeIndex < 3) {
    float nextSplit = scene.cascadeSplits[cascadeIndex];
    float cascadeWidth = nextSplit - (cascadeIndex > 0 ? scene.cascadeSplits[cascadeIndex - 1] : scene.nearClip);
    float transitionRange = cascadeWidth * 0.1; // 10% transition
    float diff = nextSplit - depth;
    
    if (diff < transitionRange) {
      float nextShadow = sampleShadow(worldPos, normal, lightDir, cascadeIndex + 1, nDotL, angle);
      shadow = mix(nextShadow, shadow, diff / transitionRange);
    }
  }

  return shadow;
}

void main() {
  // DEBUG CHECK REMOVED - RESTORING PBR LOGIC

  Material mat =
      allMaterialBuffers[pc.materialBufferIndex].materials[inMaterialIndex];
  SceneData scene = allSceneBuffers[pc.sceneDataIndex].scene;

  vec3 baseColor = mat.baseColorFactor.rgb;
  float alpha = mat.baseColorFactor.a;
  
  if (mat.baseColorTextureIndex != -1) {
    vec4 texColor = textureLod(textures[nonuniformEXT(mat.baseColorTextureIndex)], inUV, 0.0);
    baseColor *= texColor.rgb;
    alpha *= texColor.a;
  }
  
  // Alpha Masking
  if (mat.alphaMode == 1) { // MASK
      if (alpha < mat.alphaCutoff) {
          discard;
      }
  } else if (mat.alphaMode == 2) { // BLEND
      if (alpha < 0.01) {
          discard;
      }
  }

  // Alpha Blending logic (if supported) not really possible in deferred/forward without sorting, 
  // but for forward we just use alpha.
  // Assuming Forward pipeline if we are writing to outFragColor directly? 
  // Or is it Deferred G-Buffer? 
  // outputs: 0=Color, 1=Normal, 2=Velocity. Looks like Forward with G-Buffer output (Hybrid) or simple Forward.
  // Actually existing shader writes `outFragColor = vec4(color, 1.0);` at line 434 (old line).
  // So it's opaque forward. 
  // We can't do real transparency without setting alpha output or sorting.
  // But let's at least support MASK.

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

  vec3 emissive = mat.emissiveFactor.rgb;
  if (mat.emissiveTextureIndex != -1) {
    emissive *=
        textureLod(textures[nonuniformEXT(mat.emissiveTextureIndex)], inUV, 0.0)
            .rgb;
  }
  emissive *= mat.emissiveFactor.a;

  // HDR and Tonemapping
  vec3 color = ambient + lo + emissive;

  // Transmission
  float transmission = mat.transmissionFactor;
  if (mat.transmissionTextureIndex != -1) {
    transmission *= textureLod(textures[nonuniformEXT(mat.transmissionTextureIndex)], inUV, 0.0).r;
  }

  if (transmission > 0.0 && scene.sceneColorIndex != -1) {
    vec2 screenUV = gl_FragCoord.xy / vec2(scene.screenWidth, scene.screenHeight);
    
    // Simple refraction based on normal and IOR
    // real refraction requires depth tracing or raymarching, here we approximate offset
    vec3 viewDir = normalize(scene.cameraPos.xyz - inWorldPos);
    float ior = mat.ior;
    if (ior == 0.0) ior = 1.5;
    float eta = 1.0 / ior;
    vec3 refracted = refract(-viewDir, N, eta);
    vec2 offset = refracted.xy * (mat.thicknessFactor > 0.0 ? mat.thicknessFactor : 1.0) * 0.1;
    
    // Limits
    screenUV += offset * (1.0 - roughness); 

    vec3 transmittedColor = texture(textures[nonuniformEXT(scene.sceneColorIndex)], screenUV).rgb;

    // Volume / Thickness (Beer's Law approximation)
    if (mat.thicknessFactor > 0.0) {
        vec3 attenuationColor = mat.baseColorFactor.rgb; // Use base color as attenuation hint
        float thickness = mat.thicknessFactor; // Should sample thickness map if present
         if (mat.thicknessTextureIndex != -1) {
             thickness *= texture(textures[nonuniformEXT(mat.thicknessTextureIndex)], inUV).g;
         }
        vec3 absorption = exp(-((vec3(1.0) - attenuationColor) * thickness));
        transmittedColor *= absorption;
    }

    // Blend: Linear interpolation for now (Replacing diffuse with transmission)
    // To preserve specular, we should ideally add efficient specular, but here we mix.
    // For physically correct, we should separate Diffuse and Specular lobes.
    // Hack: Add specular on top? lo already has it.
    // Let's assume color ~= Diffuse + Specular.
    // We want (1-T)*Diffuse + T*Transmitted + Specular.
    // Current 'color' is (Diffuse + Specular).
    // So mix(color, transmitted, T) dampens specular.
    // We will just mix for this pass.
    
    color = mix(color, transmittedColor, transmission);
  }

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
