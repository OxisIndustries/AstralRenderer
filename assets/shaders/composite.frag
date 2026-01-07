#version 460
#extension GL_EXT_nonuniform_qualifier : enable

layout(location = 0) in vec2 inUV;
layout(location = 0) out vec4 outColor;

layout(set = 0, binding = 0) uniform sampler2D textures[];

layout(push_constant) uniform PushConstants {
    int hdrTextureIndex;
    int bloomTextureIndex;
    int ssaoTextureIndex;
    float exposure;
    float bloomStrength;
    int enableSSAO;
} pc;

// Narkowicz 2015, "ACES Filmic Tone Mapping Curve"
vec3 ACESFilm(vec3 x) {
    float a = 2.51;
    float b = 0.03;
    float c = 2.43;
    float d = 0.59;
    float e = 0.14;
    return clamp((x * (a * x + b)) / (x * (c * x + d) + e), 0.0, 1.0);
}

void main() {
    vec3 hdrColor = texture(textures[nonuniformEXT(pc.hdrTextureIndex)], inUV).rgb;
    
    if (pc.enableSSAO == 1 && pc.ssaoTextureIndex != -1) {
        float ssao = texture(textures[nonuniformEXT(pc.ssaoTextureIndex)], inUV).r;
        hdrColor *= ssao;
    }

    hdrColor *= pc.exposure;

    if (pc.bloomTextureIndex != -1) {
        vec3 bloomColor = texture(textures[nonuniformEXT(pc.bloomTextureIndex)], inUV).rgb;
        hdrColor += bloomColor * pc.bloomStrength;
    }

    // Tone mapping
    vec3 mapped = ACESFilm(hdrColor);

    // Gamma correction
    mapped = pow(mapped, vec3(1.0 / 2.2));

    outColor = vec4(mapped, 1.0);
}
