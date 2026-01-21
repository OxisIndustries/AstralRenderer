#version 460
#extension GL_EXT_nonuniform_qualifier : enable

layout(location = 0) out float outColor;

layout(push_constant) uniform PushConstants {
    int inputTextureIndex;
    int depthTextureIndex;
    float sharpness; // Ignored for now, logic hardcoded
} pc;

layout(set = 0, binding = 0) uniform sampler2D textures[];

layout(location = 0) in vec2 inUV;

// Helper to linearize depth if needed, but for bilateral weight raw depth delta often works if small enough.
// Actually, raw depth is non-linear. The 'sharpness' or weight function needs to account for this.
// A simpler robust way is comparing values directly.
float getDepth(vec2 uv) {
    return texture(textures[nonuniformEXT(pc.depthTextureIndex)], uv).r;
}

void main() {
    float centerDepth = getDepth(inUV);
    // If background, don't blur? Or just blur. SSAO is 1.0 there logic-wise.
    
    vec2 texelSize = 1.0 / vec2(textureSize(textures[nonuniformEXT(pc.inputTextureIndex)], 0));
    float result = 0.0;
    float weightSum = 0.0;
    
    const int kernelResult = 2; // radius
    
    for (int x = -kernelResult; x <= kernelResult; ++x) {
        for (int y = -kernelResult; y <= kernelResult; ++y) {
            vec2 offset = vec2(float(x), float(y)) * texelSize;
            vec2 sampleUV = inUV + offset;
            
            float sampleDepth = getDepth(sampleUV);
            
            // Gaussian weight for spatial
            float spatialWeight = exp(-(float(x*x + y*y)) / 4.0); // sigma ~ 1.5
            
            // Depth weight for bilateral
            // Tuning this is sensitive. 
            // Difference in raw depth buffer:
            // Close objects: differences are large? No, diffs are large in linearized Z.
            // Raw depth [0,1].
            float depthDiff = abs(centerDepth - sampleDepth);
            // Sharpness: 
            // If we use raw depth: 
            // - Close to camera, precision is high. Diffs are small?
            // - Far from camera, precision low.
            // Let's use a heuristic: if diff > threshold, weight = 0.
            // Threshold usually 0.001 - 0.05 range for raw depth depending on scene scale. 
            // Better to strictly reject if edge is detected.
            float rangeWeight = smoothstep(0.000, 0.002, depthDiff); // 1.0 if diff is large, 0.0 if diff is small? 
            // We WANT: 1.0 if diff is SMALL, 0.0 if diff is LARGE.
            // So: 1.0 - smoothstep(...)
            // Let's use standard exponential falloff
            float depthWeight = exp(-depthDiff * 1000.0); // Adjust 1000.0 based on scene scale
            
            float w = spatialWeight * depthWeight;
            
            result += texture(textures[nonuniformEXT(pc.inputTextureIndex)], sampleUV).r * w;
            weightSum += w;
        }
    }
    
    if (weightSum > 0.0)
        outColor = result / weightSum;
    else
        outColor = texture(textures[nonuniformEXT(pc.inputTextureIndex)], inUV).r;
}
