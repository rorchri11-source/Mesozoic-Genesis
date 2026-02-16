#version 450

layout(location = 0) in vec4 fragColor;
layout(location = 1) in vec3 fragNormal;
layout(location = 2) in vec3 fragWorldPos;
layout(location = 3) in float fragTime;
layout(location = 4) in vec3 fragCameraPos;
layout(location = 5) in vec2 fragUV;
layout(location = 6) in vec4 fragSplat; // From Vertex (Set 0, Binding 1 sample)

layout(location = 0) out vec4 outColor;

const vec3 L = normalize(vec3(0.5, 1.0, 0.4));
const vec3 SUN_COLOR = vec3(1.4, 1.3, 1.1);
const vec3 AMBIENT = vec3(0.2, 0.25, 0.35);

// Simple hash for micro-detail
float hash12(vec2 p) {
    vec3 p3 = fract(vec3(p.xyx) * 0.1031);
    p3 += dot(p3, p3.yzx + 33.33);
    return fract((p3.x + p3.y) * p3.z);
}

void main() {
    float dist = length(fragWorldPos - fragCameraPos);
    vec3 rd = normalize(fragWorldPos - fragCameraPos);

    // --- SKY ---
    if (fragColor.a < 0.01) {
        float h = max(rd.y, 0.0);
        vec3 sky = mix(vec3(0.55, 0.75, 1.0), vec3(0.08, 0.15, 0.4), pow(h, 0.45));
        outColor = vec4(sky, 1.0);
        return;
    }

    vec3 N = normalize(fragNormal);
    float NdotL = max(dot(N, L), 0.0);
    float diff = NdotL * 0.7 + 0.3;

    vec3 albedo;

    // --- GRASS ---
    if (abs(fragColor.a - 0.5) < 0.1) {
        // Gradient: Dark base -> Light tip
        vec3 greenBase = vec3(0.02, 0.15, 0.01);
        vec3 greenTip = vec3(0.08, 0.45, 0.05);
        albedo = mix(greenBase, greenTip, fragUV.y);
        
        // AO at bottom
        albedo *= mix(0.5, 1.2, fragUV.y);

        // Blend with underlying terrain color (Dirt/Rock influence)
        // splat.g = Dirt, splat.b = Rock
        vec3 dirty = vec3(0.2, 0.15, 0.1);
        albedo = mix(albedo, dirty, fragSplat.g * 0.4); 
    }
    // --- TERRAIN & DINOS ---
    else {
        // Use Splat Map mixing
        // R = Grass, G = Dirt, B = Rock
        vec3 grassCol = vec3(0.12, 0.28, 0.08); // Matches grass base
        vec3 dirtCol = vec3(0.3, 0.22, 0.14);
        vec3 rockCol = vec3(0.45, 0.4, 0.38);

        vec3 terrainCol = fragSplat.r * grassCol + 
                          fragSplat.g * dirtCol + 
                          fragSplat.b * rockCol;
        
        // Normalize if needed (alpha channel might be useful later)
        // Add micro-noise
        float noiseVal = hash12(fragWorldPos.xz * 10.0);
        terrainCol *= (0.9 + noiseVal * 0.15);

        albedo = terrainCol;
    }

    vec3 direct = SUN_COLOR * diff;
    vec3 finalColor = albedo * (direct + AMBIENT);

    // Fog
    float fog = clamp(1.0 - exp(-dist * 0.0004), 0.0, 1.0);
    finalColor = mix(finalColor, vec3(0.6, 0.75, 0.9), fog);

    // ACES Tonemapping (Simplified)
    // finalColor = finalColor / (finalColor + vec3(1.0)); // simple Reinhard
    // Apply gamma
    finalColor = pow(finalColor, vec3(1.0/2.2));

    outColor = vec4(finalColor, 1.0);
}
