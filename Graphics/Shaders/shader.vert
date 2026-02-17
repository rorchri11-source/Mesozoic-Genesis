#version 450

layout(push_constant) uniform PushConstants {
    mat4 renderMatrix; // MVP
    vec4 color;
    float time;
    vec3 cameraPos;
    vec3 modelPos;
    // Morphing
    vec4 morphWeights; // x=Snout, y=Bulk, z=Horn
    uint vertexCount;
} push;

struct MorphDelta {
    vec4 pos;
    vec4 norm;
};

// New Texture Bindings (Set 0)
layout(set = 0, binding = 0) uniform sampler2D heightMap;
layout(set = 0, binding = 1) uniform sampler2D splatMap;
layout(std430, set = 0, binding = 2) readonly buffer MorphDeltas {
    MorphDelta deltas[];
};

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec2 inUV;
layout(location = 2) in vec3 inNormal;
layout(location = 3) in vec4 inTangent;

layout(location = 0) out vec4 fragColor;
layout(location = 1) out vec3 fragNormal;
layout(location = 2) out vec3 fragWorldPos;
layout(location = 3) out float fragTime;
layout(location = 4) out vec3 fragCameraPos;
layout(location = 5) out vec2 fragUV;
layout(location = 6) out vec4 fragSplat; // Pass splat to fragment shader

// --- Instance hashing ---
float instanceHash(uint n) {
    n = (n << 13U) ^ n;
    n = n * (n * n * 15731U + 789221U) + 1376312589U;
    return float(n & 0x7fffffffU) / float(0x7fffffff);
}

void main() {
    // Map Constraints
    float mapSize = 1536.0; // 1024 * 1.5
    float halfMap = mapSize * 0.5;

    // ========================================
    // PATH A: SKYBOX (alpha < 0.01)
    // ========================================
    if (push.color.a < 0.01) {
        vec3 skyWorldPos = inPosition + push.cameraPos;
        gl_Position = (push.renderMatrix * vec4(skyWorldPos, 1.0)).xyww;
        fragColor = push.color;
        fragNormal = inNormal;
        fragWorldPos = skyWorldPos;
        fragTime = push.time;
        fragCameraPos = push.cameraPos;
        fragUV = inUV;
        return;
    }

    // ========================================
    // PATH B: GRASS (alpha ~0.5)
    // ========================================
    if (abs(push.color.a - 0.5) < 0.05) {
        uint id = uint(gl_InstanceIndex);

        float r1 = instanceHash(id * 1678U + 11U);
        float r2 = instanceHash(id * 9821U + 33U);
        float r3 = instanceHash(id * 5432U + 77U);
        float r4 = instanceHash(id * 1122U + 99U);

        // Position on map
        float grassX = r1 * mapSize - halfMap;
        float grassZ = r2 * mapSize - halfMap;

        // Sample Height & Splat from Texture
        vec2 uv = (vec2(grassX, grassZ) + halfMap) / mapSize;
        
        // Safety Clamp UV
        uv = clamp(uv, 0.001, 0.999);

        float h = texture(heightMap, uv).r;
        vec4 splat = texture(splatMap, uv);

        // Context Cull: No grass on Rock (Blue > 0.5) or if Grass Weight (Red) is low
        if (splat.b > 0.5 || splat.r < 0.2) {
            gl_Position = vec4(0,0,0,0);
            return;
        }

        vec3 grassWorldPos = vec3(grassX, h, grassZ);

        // Distance cull (500m)
        float camDist = distance(grassWorldPos.xz, push.cameraPos.xz);
        if (camDist > 500.0) {
            gl_Position = vec4(0,0,0,0);
            return;
        }

        // Procedural Animation & Variation
        float rot = r3 * 6.28318;
        float sr = sin(rot);
        float cr = cos(rot);

        vec3 lp = inPosition;

        // Apply Morphing (if active)
        if (push.vertexCount > 0) {
            uint vid = gl_VertexIndex;
            // Target Snout
            if (push.morphWeights.x > 0.0) {
                MorphDelta d = deltas[0 * push.vertexCount + vid];
                lp += d.pos.xyz * push.morphWeights.x;
            }
            // Target Bulk
            if (push.morphWeights.y > 0.0) {
                MorphDelta d = deltas[1 * push.vertexCount + vid];
                lp += d.pos.xyz * push.morphWeights.y;
            }
            // Target Horn
            if (push.morphWeights.z > 0.0) {
                MorphDelta d = deltas[2 * push.vertexCount + vid];
                lp += d.pos.xyz * push.morphWeights.z;
            }
        }
        
        // Rotate around Y
        float rpx = lp.x * cr - lp.z * sr;
        float rpz = lp.x * sr + lp.z * cr;
        lp.x = rpx;
        lp.z = rpz;

        // Scale Variation
        lp *= (0.8 + r4 * 1.2);

        // Wind Animation
        float hf = inUV.y * inUV.y; // Sway top more
        float wind = sin(push.time * 2.0 + grassWorldPos.x * 0.5 + grassWorldPos.z * 0.5) * 0.2;
        lp.x += wind * hf;

        // Player Interaction (Push)
        float pDist = distance(grassWorldPos.xz, push.cameraPos.xz);
        if (pDist < 3.0) {
            vec2 pushDir = normalize(grassWorldPos.xz - push.cameraPos.xz);
            float force = (3.0 - pDist) / 3.0;
            lp.x += pushDir.x * force * 1.5 * hf;
            lp.z += pushDir.y * force * 1.5 * hf;
            lp.y -= force * 0.3 * hf;
        }

        vec3 finalPos = grassWorldPos + lp;
        gl_Position = push.renderMatrix * vec4(finalPos, 1.0);

        fragNormal = vec3(0.0, 1.0, 0.0);
        // Grass Color: Green modulated by splat map variation?
        // Let fragment shader handle refined color.
        fragColor = push.color; 
        fragWorldPos = finalPos;
        fragTime = push.time;
        fragCameraPos = push.cameraPos;
        fragUV = inUV;
        fragSplat = splat; // Pass to frag shader
        return;
    }

    // ========================================
    // PATH C: EVERYTHING ELSE (Terrain, Dinos)
    // ========================================
    vec3 morphedPos = inPosition;
    vec3 morphedNorm = inNormal;

    // Apply Morphing (Dinos)
    if (push.vertexCount > 0) {
        uint vid = gl_VertexIndex;
        vec3 dP = vec3(0);
        vec3 dN = vec3(0);

        if (push.morphWeights.x > 0.0) {
            MorphDelta d = deltas[0 * push.vertexCount + vid];
            dP += d.pos.xyz * push.morphWeights.x;
            dN += d.norm.xyz * push.morphWeights.x;
        }
        if (push.morphWeights.y > 0.0) {
            MorphDelta d = deltas[1 * push.vertexCount + vid];
            dP += d.pos.xyz * push.morphWeights.y;
            dN += d.norm.xyz * push.morphWeights.y;
        }
        if (push.morphWeights.z > 0.0) {
            MorphDelta d = deltas[2 * push.vertexCount + vid];
            dP += d.pos.xyz * push.morphWeights.z;
            dN += d.norm.xyz * push.morphWeights.z;
        }
        morphedPos += dP;
        morphedNorm = normalize(morphedNorm + dN);
    }

    vec3 worldPos = push.modelPos + morphedPos;
    
    // Calculate UV for terrain based on worldPos if needed
    // But existing inUV is likely tiling UV. We need macro UV for splat map.
    // If this is terrain (how do we know? push.modelPos is 0?), calculate splat UV
    // Assuming terrain is centered at 0.
    vec2 splatUV = (worldPos.xz + halfMap) / mapSize;
    // For non-terrain objects, this UV might be nonsense but harmless if unused.
    
    gl_Position = push.renderMatrix * vec4(morphedPos, 1.0); // Wait, previous code used inPosition for logic but worldPos for lighting?
    // Correction: Standard Model rendering applies Model Matrix inside renderMatrix?
    // No, push.renderMatrix includes MVP. 
    // Usually MVP = Projection * View * Model.
    // So inPosition (local) is correct for gl_Position.

    fragColor = push.color;
    fragNormal = morphedNorm;
    fragWorldPos = worldPos;
    fragTime = push.time;
    fragCameraPos = push.cameraPos;
    fragUV = inUV;
    fragSplat = texture(splatMap, splatUV); // Sample splat for terrain
}
