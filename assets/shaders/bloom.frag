#version 460
#extension GL_EXT_nonuniform_qualifier : enable

layout(location = 0) in vec2 inUV;
layout(location = 0) out vec4 outColor;

layout(set = 0, binding = 0) uniform sampler2D textures[];

layout(push_constant) uniform PushConstants {
    int inputTextureIndex;
    int mode; // 0: Bright Extraction, 1: Horizontal Blur, 2: Vertical Blur
    float threshold;
    float softness;
} pc;

vec3 BrightExtraction(vec3 color) {
    float brightness = dot(color, vec3(0.2126, 0.7152, 0.0722));
    
    // Soft thresholding (Karis curve)
    float knee = pc.threshold * pc.softness;
    float soft = brightness - pc.threshold + knee;
    soft = clamp(soft, 0.0, 2.0 * knee);
    soft = soft * soft / (4.0 * knee + 0.00001);
    
    float contribution = max(soft, brightness - pc.threshold);
    contribution /= max(brightness, 0.00001);
    
    return color * contribution;
}

// Optimized 9-tap Gaussian blur (2nd pass needs to be higher quality)
vec3 Blur(bool horizontal) {
    vec2 texSize = textureSize(textures[nonuniformEXT(pc.inputTextureIndex)], 0);
    vec2 offset = (horizontal ? vec2(1.0 / texSize.x, 0.0) : vec2(0.0, 1.0 / texSize.y));
    
    float weights[5] = float[](0.227027, 0.1945946, 0.1216216, 0.054054, 0.016216);
    
    vec3 result = texture(textures[nonuniformEXT(pc.inputTextureIndex)], inUV).rgb * weights[0];
    for(int i = 1; i < 5; ++i) {
        result += texture(textures[nonuniformEXT(pc.inputTextureIndex)], inUV + offset * i).rgb * weights[i];
        result += texture(textures[nonuniformEXT(pc.inputTextureIndex)], inUV - offset * i).rgb * weights[i];
    }
    return result;
}

void main() {
    if (pc.mode == 0) {
        vec3 color = texture(textures[nonuniformEXT(pc.inputTextureIndex)], inUV).rgb;
        outColor = vec4(BrightExtraction(color), 1.0);
    } else if (pc.mode == 1) {
        outColor = vec4(Blur(true), 1.0);
    } else {
        outColor = vec4(Blur(false), 1.0);
    }
}
