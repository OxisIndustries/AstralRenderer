#version 450
#extension GL_EXT_nonuniform_qualifier : enable

layout(location = 0) in vec2 inUV;
layout(location = 0) out vec4 outColor;

layout(push_constant) uniform PushConstants {
    int currentIdx;
    int historyIdx;
    int velocityIdx;
    int depthIdx;
} pc;

layout(set = 0, binding = 0) uniform sampler2D allTextures[];

// Neighborhood clamping for ghosting reduction
vec3 clip_aabb(vec3 q, vec3 aabb_min, vec3 aabb_max) {
    vec3 p_clip = 0.5 * (aabb_max + aabb_min);
    vec3 e_clip = 0.5 * (aabb_max - aabb_min) + 0.00000001;

    vec3 v_clip = q - p_clip;
    vec3 v_unit = v_clip.xyz / e_clip;
    vec3 a_unit = abs(v_unit);
    float ma_unit = max(a_unit.x, max(a_unit.y, a_unit.z));

    if (ma_unit > 1.0)
        return p_clip + v_clip / ma_unit;
    else
        return q;
}

void main() {
    vec3 current = texture(allTextures[nonuniformEXT(pc.currentIdx)], inUV).rgb;
    vec2 velocity = texture(allTextures[nonuniformEXT(pc.velocityIdx)], inUV).xy;
    
    vec2 prevUV = inUV - velocity;
    
    // Out of bounds check for history
    if (prevUV.x < 0.0 || prevUV.x > 1.0 || prevUV.y < 0.0 || prevUV.y > 1.0) {
        outColor = vec4(current, 1.0);
        return;
    }

    vec3 history = texture(allTextures[nonuniformEXT(pc.historyIdx)], prevUV).rgb;

    // Simple neighborhood clamping
    vec3 m1 = vec3(0.0);
    vec3 m2 = vec3(0.0);
    
    vec2 texelSize = 1.0 / textureSize(allTextures[nonuniformEXT(pc.currentIdx)], 0);

    for(int x = -1; x <= 1; ++x) {
        for(int y = -1; y <= 1; ++y) {
            vec3 c = texture(allTextures[nonuniformEXT(pc.currentIdx)], inUV + vec2(x, y) * texelSize).rgb;
            m1 += c;
            m2 += c * c;
        }
    }

    vec3 mean = m1 / 9.0;
    vec3 stddev = sqrt(max(vec3(0.0), (m2 / 9.0) - (mean * mean)));
    
    float k = 1.0; // Variance scale
    vec3 min_c = mean - k * stddev;
    vec3 max_c = mean + k * stddev;

    history = clip_aabb(history, min_c, max_c);

    float alpha = 0.1; // Feedback factor
    vec3 result = mix(history, current, alpha);

    outColor = vec4(result, 1.0);
}
