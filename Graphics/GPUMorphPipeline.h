#pragma once
#include <array>
#include <cmath>
#include <cstdint>
#include <string>
#include <vector>


namespace Mesozoic {
namespace Graphics {

// =========================================================================
// Compute Shader definitions for GPU-side morphing
// These mirror GLSL/HLSL compute shaders but are defined as C++ structs
// for CPU-side data management and staging buffer uploads
// =========================================================================

// Push constant data for the morph compute shader
struct MorphComputePushConstants {
  uint32_t vertexCount;
  uint32_t targetCount;
  uint32_t padding[2];
};

// Per-entity morph dispatch data
struct MorphDispatch {
  uint32_t entityId;
  uint32_t baseVertexOffset; // Offset into global vertex buffer
  uint32_t vertexCount;
  uint32_t morphTargetOffset; // Offset into morph delta buffer
  float weights[16];          // Up to 16 morph targets per entity
};

// =========================================================================
// GPU Morphing Pipeline
// Runs as a Compute Shader before the vertex stage
// =========================================================================
class GPUMorphPipeline {
public:
  // Buffers (managed by VulkanBackend)
  // inputVertexBuffer:  Base mesh vertices (read-only)
  // morphDeltaBuffer:   All morph deltas packed (read-only)
  // outputVertexBuffer: Deformed vertices (write, used as VBO)
  // dispatchBuffer:     Per-entity morph dispatch data

  std::vector<MorphDispatch> dispatches;

  void PrepareDispatch(uint32_t entityId, uint32_t baseOffset,
                       uint32_t vertCount, uint32_t morphOffset,
                       const std::vector<float> &weights) {
    MorphDispatch d;
    d.entityId = entityId;
    d.baseVertexOffset = baseOffset;
    d.vertexCount = vertCount;
    d.morphTargetOffset = morphOffset;

    for (int i = 0; i < 16; ++i) {
      d.weights[i] = (i < static_cast<int>(weights.size())) ? weights[i] : 0.0f;
    }
    dispatches.push_back(d);
  }

  void Execute() {
    // In real Vulkan:
    // vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE,
    // morphComputePipeline); vkCmdBindDescriptorSets(cmd,
    // VK_PIPELINE_BIND_POINT_COMPUTE, ...);
    //
    // for (auto& d : dispatches) {
    //     vkCmdPushConstants(cmd, layout, VK_SHADER_STAGE_COMPUTE_BIT, 0,
    //     sizeof(d), &d); uint32_t groupCount = (d.vertexCount + 63) / 64; //
    //     64 threads per group vkCmdDispatch(cmd, groupCount, 1, 1);
    // }
    //
    // // Memory barrier: compute write -> vertex read
    // VkMemoryBarrier barrier{};
    // barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
    // barrier.dstAccessMask = VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT;
    // vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
    //     VK_PIPELINE_STAGE_VERTEX_INPUT_BIT, 0, 1, &barrier, 0, nullptr, 0,
    //     nullptr);
  }

  void Clear() { dispatches.clear(); }

  // =====================================================================
  // GLSL Compute Shader source (embedded as string for runtime compilation)
  // =====================================================================
  static const char *GetComputeShaderSource() {
    return R"(
                #version 450
                
                layout(local_size_x = 64) in;

                struct Vertex {
                    vec3 position;
                    vec3 normal;
                    vec4 tangent;
                    vec2 uv;
                    uvec4 boneIndices;
                    vec4 boneWeights;
                };

                struct MorphDelta {
                    vec3 positionDelta;
                    vec3 normalDelta;
                };

                layout(std430, binding = 0) readonly buffer BaseVertices {
                    Vertex baseVerts[];
                };

                layout(std430, binding = 1) readonly buffer MorphDeltas {
                    MorphDelta deltas[];
                };

                layout(std430, binding = 2) writeonly buffer OutputVertices {
                    Vertex outVerts[];
                };

                layout(push_constant) uniform PushConstants {
                    uint vertexCount;
                    uint targetCount;
                    uint baseOffset;
                    uint deltaOffset;
                    float weights[16];
                } pc;

                void main() {
                    uint gid = gl_GlobalInvocationID.x;
                    if (gid >= pc.vertexCount) return;

                    uint vid = pc.baseOffset + gid;
                    Vertex v = baseVerts[vid];

                    // Apply morph targets: V_final = V_base + Sum(Delta_i * Weight_i)
                    for (uint t = 0; t < pc.targetCount && t < 16; ++t) {
                        float w = pc.weights[t];
                        if (abs(w) < 0.001) continue;

                        uint did = pc.deltaOffset + t * pc.vertexCount + gid;
                        MorphDelta d = deltas[did];

                        v.position += d.positionDelta * w;
                        v.normal += d.normalDelta * w;
                    }

                    v.normal = normalize(v.normal);
                    outVerts[vid] = v;
                }
            )";
  }
};

// =========================================================================
// GLSL Fragment Shader for PBR + SSS + Dynamic Scars
// =========================================================================
class ShaderSources {
public:
  static const char *GetGBufferVertexShader() {
    return R"(
                #version 450

                layout(location = 0) in vec3 inPosition;
                layout(location = 1) in vec3 inNormal;
                layout(location = 2) in vec4 inTangent;
                layout(location = 3) in vec2 inUV;
                layout(location = 4) in uvec4 inBoneIndices;
                layout(location = 5) in vec4 inBoneWeights;

                layout(set = 0, binding = 0) uniform SceneUBO {
                    mat4 viewProjection;
                    vec3 cameraPos;
                    float time;
                    vec3 sunDir;
                    float sunIntensity;
                } scene;

                layout(push_constant) uniform PushConstants {
                    mat4 model;
                } push;

                // Bone animation texture
                layout(set = 1, binding = 0) uniform sampler2D boneTexture;

                layout(location = 0) out vec3 fragWorldPos;
                layout(location = 1) out vec3 fragNormal;
                layout(location = 2) out vec2 fragUV;
                layout(location = 3) out vec4 fragTangent;

                mat4 getBoneMatrix(uint boneIndex, float animTime) {
                    // Read 4 texels from bone texture to reconstruct 4x4 matrix
                    float row = animTime;
                    mat4 m;
                    m[0] = texelFetch(boneTexture, ivec2(boneIndex * 4 + 0, int(row)), 0);
                    m[1] = texelFetch(boneTexture, ivec2(boneIndex * 4 + 1, int(row)), 0);
                    m[2] = texelFetch(boneTexture, ivec2(boneIndex * 4 + 2, int(row)), 0);
                    m[3] = texelFetch(boneTexture, ivec2(boneIndex * 4 + 3, int(row)), 0);
                    return m;
                }

                void main() {
                    // Skeletal animation
                    mat4 skinMatrix = 
                        getBoneMatrix(inBoneIndices.x, scene.time) * inBoneWeights.x +
                        getBoneMatrix(inBoneIndices.y, scene.time) * inBoneWeights.y +
                        getBoneMatrix(inBoneIndices.z, scene.time) * inBoneWeights.z +
                        getBoneMatrix(inBoneIndices.w, scene.time) * inBoneWeights.w;

                    vec4 skinnedPos = skinMatrix * vec4(inPosition, 1.0);
                    vec4 worldPos = push.model * skinnedPos;

                    fragWorldPos = worldPos.xyz;
                    fragNormal = normalize(mat3(push.model) * mat3(skinMatrix) * inNormal);
                    fragUV = inUV;
                    fragTangent = inTangent;

                    gl_Position = scene.viewProjection * worldPos;
                }
            )";
  }

  static const char *GetGBufferFragmentShader() {
    return R"(
                #version 450

                layout(location = 0) in vec3 fragWorldPos;
                layout(location = 1) in vec3 fragNormal;
                layout(location = 2) in vec2 fragUV;
                layout(location = 3) in vec4 fragTangent;

                // GBuffer outputs
                layout(location = 0) out vec4 outAlbedo;     // RGB = albedo, A = metallic
                layout(location = 1) out vec4 outNormal;     // RGB = world normal, A = roughness
                layout(location = 2) out vec4 outEmission;   // RGB = emission, A = SSS mask

                layout(set = 2, binding = 0) uniform sampler2D albedoTex;
                layout(set = 2, binding = 1) uniform sampler2D normalTex;
                layout(set = 2, binding = 2) uniform sampler2D pbrTex; // R=roughness, G=metallic, B=AO

                layout(set = 2, binding = 3) uniform MaterialUBO {
                    vec3 albedoColor;
                    float roughness;
                    float metallic;
                    float sssStrength;
                    vec3 sssColor;
                    float scaleSize;
                    int scaleSeed;
                } mat;

                // Texture bombing for micro-scales
                vec2 textureBomb(vec2 uv, float scale, int seed) {
                    vec2 cell = floor(uv * scale);
                    vec2 localUV = fract(uv * scale);
                    // Random offset per cell for variation
                    float h = fract(sin(dot(cell + float(seed), vec2(127.1, 311.7))) * 43758.5453);
                    return localUV + vec2(h * 0.1, h * 0.15);
                }

                void main() {
                    // Sample textures with scale bombing
                    vec2 scaleUV = textureBomb(fragUV, mat.scaleSize, mat.scaleSeed);
                    vec3 albedo = texture(albedoTex, scaleUV).rgb * mat.albedoColor;
                    vec3 normalMap = texture(normalTex, scaleUV).rgb * 2.0 - 1.0;
                    vec3 pbr = texture(pbrTex, fragUV).rgb;

                    // TBN matrix for normal mapping
                    vec3 T = normalize(fragTangent.xyz);
                    vec3 N = normalize(fragNormal);
                    vec3 B = cross(N, T) * fragTangent.w;
                    mat3 TBN = mat3(T, B, N);
                    vec3 worldNormal = normalize(TBN * normalMap);

                    // Output GBuffer
                    outAlbedo = vec4(albedo, mat.metallic * pbr.g);
                    outNormal = vec4(worldNormal * 0.5 + 0.5, mat.roughness * pbr.r);
                    outEmission = vec4(0.0, 0.0, 0.0, mat.sssStrength); // SSS mask in alpha
                }
            )";
  }

  static const char *GetLightingFragmentShader() {
    return R"(
                #version 450
                
                // Fullscreen quad: reconstruct world position from depth
                layout(set = 0, binding = 0) uniform sampler2D gAlbedo;
                layout(set = 0, binding = 1) uniform sampler2D gNormal;
                layout(set = 0, binding = 2) uniform sampler2D gDepth;
                layout(set = 0, binding = 3) uniform sampler2D shadowMap;

                layout(location = 0) in vec2 fragUV;
                layout(location = 0) out vec4 outColor;

                const float PI = 3.14159265359;

                // Cook-Torrance BRDF components
                float DistributionGGX(vec3 N, vec3 H, float roughness) {
                    float a = roughness * roughness;
                    float a2 = a * a;
                    float NdotH = max(dot(N, H), 0.0);
                    float denom = (NdotH * NdotH * (a2 - 1.0) + 1.0);
                    return a2 / (PI * denom * denom);
                }

                float GeometrySmith(float NdotV, float NdotL, float roughness) {
                    float r = roughness + 1.0;
                    float k = (r * r) / 8.0;
                    float ggx1 = NdotV / (NdotV * (1.0 - k) + k);
                    float ggx2 = NdotL / (NdotL * (1.0 - k) + k);
                    return ggx1 * ggx2;
                }

                vec3 FresnelSchlick(float cosTheta, vec3 F0) {
                    return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
                }

                void main() {
                    vec4 albedoData = texture(gAlbedo, fragUV);
                    vec4 normalData = texture(gNormal, fragUV);
                    
                    vec3 albedo = albedoData.rgb;
                    float metallic = albedoData.a;
                    vec3 N = normalize(normalData.rgb * 2.0 - 1.0);
                    float roughness = normalData.a;

                    // Simplified: single directional light
                    vec3 L = normalize(vec3(0.5, 1.0, 0.3));
                    vec3 V = normalize(vec3(0, 0, 1)); // Camera direction placeholder
                    vec3 H = normalize(V + L);

                    float NdotL = max(dot(N, L), 0.0);
                    float NdotV = max(dot(N, V), 0.0);

                    vec3 F0 = mix(vec3(0.04), albedo, metallic);
                    
                    float D = DistributionGGX(N, H, roughness);
                    float G = GeometrySmith(NdotV, NdotL, roughness);
                    vec3 F = FresnelSchlick(max(dot(H, V), 0.0), F0);

                    vec3 numerator = D * G * F;
                    float denominator = 4.0 * NdotV * NdotL + 0.0001;
                    vec3 specular = numerator / denominator;

                    vec3 kD = (vec3(1.0) - F) * (1.0 - metallic);
                    vec3 diffuse = kD * albedo / PI;

                    vec3 Lo = (diffuse + specular) * vec3(3.0) * NdotL; // Light color * intensity
                    vec3 ambient = vec3(0.03) * albedo;
                    
                    outColor = vec4(ambient + Lo, 1.0);
                }
            )";
  }
};

} // namespace Graphics
} // namespace Mesozoic
