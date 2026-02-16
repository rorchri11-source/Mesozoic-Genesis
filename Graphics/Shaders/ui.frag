#version 450

layout(location = 0) in vec2 fragTexCoord;
layout(location = 1) in vec4 fragColor;

layout(binding = 1) uniform sampler2D texSampler;

layout(location = 0) out vec4 outColor;

void main() {
    // Sample texture and tint
    vec4 texColor = texture(texSampler, fragTexCoord);
    
    // UI Tinting: Use vertex color (from push constant)
    outColor = texColor * fragColor;
    
    // Alpha discard check if needed, but blending handles it mostly
    if (outColor.a < 0.01) discard;
}
