#version 450

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal; // Unused
layout(location = 2) in vec2 inTexCoord;
layout(location = 3) in vec4 inTangent; // Unused
layout(location = 4) in ivec4 inBoneIndices; // Unused 
layout(location = 5) in vec4 inBoneWeights; // Unused

layout(location = 0) out vec2 fragTexCoord;
layout(location = 1) out vec4 fragColor;

layout(push_constant) uniform PushConstants {
    mat4 projection; // Ortho matrix
    vec4 position;   // x, y, sx, sy (Screen Position & Scale)
    vec4 color;      // Tint color
} pc;

void main() {
    // Quad vertices are usually defined as 0..1 or -1..1
    // Let's assume input mesh is a unit quad (0,0) to (1,1)
    
    // Scale and Translate
    vec2 pos = inPosition.xy * pc.position.zw + pc.position.xy;
    
    // Apply Ortho Projection
    gl_Position = pc.projection * vec4(pos, 0.0, 1.0);
    
    fragTexCoord = inTexCoord;
    fragColor = pc.color;
}
