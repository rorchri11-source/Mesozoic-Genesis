#pragma once
#include <iostream>
#include <string>
#include <unordered_map>
#include <vector>


namespace Mesozoic {
namespace Graphics {

// Embedded GLSL shader sources for compilation to SPIR-V
// When Vulkan SDK + glslc are available, these compile at build time.
// Otherwise, they serve as documentation and can be compiled offline.

struct ShaderSource {
  std::string name;
  std::string glsl;
  std::string entryPoint = "main";
  enum class Stage {
    Vertex,
    Fragment,
    Compute,
    Geometry,
    TessControl,
    TessEval
  } stage;
};

class ShaderLibrary {
public:
  // =====================================================================
  // VERTEX SHADERS
  // =====================================================================

  static ShaderSource GBufferVertex() {
    return {"gbuffer.vert", R"(
#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(set = 0, binding = 0) uniform SceneUBO {
    mat4 view;
    mat4 projection;
    mat4 viewProjection;
    vec4 cameraPos;
    vec4 lightDir;
    vec4 lightColor;
    float time;
    float pad1, pad2, pad3;
} scene;

layout(set = 1, binding = 0) uniform ObjectUBO {
    mat4 model;
    mat4 normalMatrix;
    uint entityId;
    float morphWeight;
    float pad1, pad2;
} object;

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inUV;
layout(location = 3) in vec4 inTangent;
layout(location = 4) in vec4 inBoneWeights;
layout(location = 5) in uvec4 inBoneIndices;

layout(location = 0) out vec3 fragWorldPos;
layout(location = 1) out vec3 fragNormal;
layout(location = 2) out vec2 fragUV;
layout(location = 3) out vec4 fragTangent;
layout(location = 4) out flat uint fragEntityId;

// Bone matrix texture (GPU animation instancing)
layout(set = 2, binding = 0) uniform sampler2D boneMatrixTex;

mat4 GetBoneMatrix(uint boneIndex) {
    float y = float(boneIndex) + 0.5;
    vec4 row0 = texelFetch(boneMatrixTex, ivec2(0, int(y)), 0);
    vec4 row1 = texelFetch(boneMatrixTex, ivec2(1, int(y)), 0);
    vec4 row2 = texelFetch(boneMatrixTex, ivec2(2, int(y)), 0);
    vec4 row3 = texelFetch(boneMatrixTex, ivec2(3, int(y)), 0);
    return mat4(row0, row1, row2, row3);
}

void main() {
    // Skeletal animation
    mat4 skinMatrix = mat4(0.0);
    skinMatrix += inBoneWeights.x * GetBoneMatrix(inBoneIndices.x);
    skinMatrix += inBoneWeights.y * GetBoneMatrix(inBoneIndices.y);
    skinMatrix += inBoneWeights.z * GetBoneMatrix(inBoneIndices.z);
    skinMatrix += inBoneWeights.w * GetBoneMatrix(inBoneIndices.w);

    // If no bones, use identity
    if (inBoneWeights.x + inBoneWeights.y + inBoneWeights.z + inBoneWeights.w < 0.01)
        skinMatrix = mat4(1.0);

    vec4 skinnedPos = skinMatrix * vec4(inPosition, 1.0);
    vec4 worldPos = object.model * skinnedPos;

    fragWorldPos = worldPos.xyz;
    fragNormal = normalize(mat3(object.normalMatrix) * mat3(skinMatrix) * inNormal);
    fragUV = inUV;
    fragTangent = vec4(normalize(mat3(object.model) * inTangent.xyz), inTangent.w);
    fragEntityId = object.entityId;

    gl_Position = scene.viewProjection * worldPos;
}
)",
            "main", ShaderSource::Stage::Vertex};
  }

  // =====================================================================
  // FRAGMENT SHADERS
  // =====================================================================

  static ShaderSource GBufferFragment() {
    return {"gbuffer.frag", R"(
#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) in vec3 fragWorldPos;
layout(location = 1) in vec3 fragNormal;
layout(location = 2) in vec2 fragUV;
layout(location = 3) in vec4 fragTangent;
layout(location = 4) in flat uint fragEntityId;

// G-Buffer outputs (MRT)
layout(location = 0) out vec4 outAlbedo;     // RGB = albedo, A = metallic
layout(location = 1) out vec4 outNormal;     // RGB = world normal, A = roughness
layout(location = 2) out vec4 outPosition;   // RGB = world pos, A = AO
layout(location = 3) out uint outEntityId;   // Entity ID for picking

// PBR Textures
layout(set = 1, binding = 1) uniform sampler2D albedoMap;
layout(set = 1, binding = 2) uniform sampler2D normalMap;
layout(set = 1, binding = 3) uniform sampler2D roughnessMetallicMap;
layout(set = 1, binding = 4) uniform sampler2D aoMap;

// SSS parameters
layout(set = 1, binding = 5) uniform MaterialUBO {
    vec4 sssColor;
    float sssStrength;
    float sssRadius;
    float scarIntensity;
    float pad;
} material;

vec3 GetNormalFromMap() {
    vec3 tangentNormal = texture(normalMap, fragUV).xyz * 2.0 - 1.0;
    vec3 N = normalize(fragNormal);
    vec3 T = normalize(fragTangent.xyz);
    vec3 B = cross(N, T) * fragTangent.w;
    mat3 TBN = mat3(T, B, N);
    return normalize(TBN * tangentNormal);
}

void main() {
    vec4 albedo = texture(albedoMap, fragUV);
    vec2 roughnessMetallic = texture(roughnessMetallicMap, fragUV).gb;
    float ao = texture(aoMap, fragUV).r;
    vec3 normal = GetNormalFromMap();

    outAlbedo = vec4(albedo.rgb, roughnessMetallic.y); // metallic in alpha
    outNormal = vec4(normal * 0.5 + 0.5, roughnessMetallic.x); // roughness in alpha
    outPosition = vec4(fragWorldPos, ao);
    outEntityId = fragEntityId;
}
)",
            "main", ShaderSource::Stage::Fragment};
  }

  static ShaderSource DeferredLightingFragment() {
    return {"deferred_lighting.frag", R"(
#version 450

layout(location = 0) in vec2 fragUV;

layout(set = 0, binding = 0) uniform sampler2D gAlbedo;
layout(set = 0, binding = 1) uniform sampler2D gNormal;
layout(set = 0, binding = 2) uniform sampler2D gPosition;
layout(set = 0, binding = 3) uniform sampler2D shadowMap;

layout(set = 0, binding = 4) uniform LightUBO {
    mat4 lightViewProj;
    vec4 lightDir;
    vec4 lightColor;
    vec4 cameraPos;
    vec4 ambientColor;
} light;

layout(location = 0) out vec4 outColor;

const float PI = 3.14159265359;

// Cook-Torrance BRDF
float DistributionGGX(vec3 N, vec3 H, float roughness) {
    float a = roughness * roughness;
    float a2 = a * a;
    float NdotH = max(dot(N, H), 0.0);
    float NdotH2 = NdotH * NdotH;
    float denom = (NdotH2 * (a2 - 1.0) + 1.0);
    denom = PI * denom * denom;
    return a2 / denom;
}

float GeometrySchlickGGX(float NdotV, float roughness) {
    float r = (roughness + 1.0);
    float k = (r * r) / 8.0;
    return NdotV / (NdotV * (1.0 - k) + k);
}

float GeometrySmith(vec3 N, vec3 V, vec3 L, float roughness) {
    return GeometrySchlickGGX(max(dot(N, V), 0.0), roughness)
         * GeometrySchlickGGX(max(dot(N, L), 0.0), roughness);
}

vec3 FresnelSchlick(float cosTheta, vec3 F0) {
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

float ShadowCalculation(vec4 fragPosLightSpace) {
    vec3 projCoords = fragPosLightSpace.xyz / fragPosLightSpace.w;
    projCoords = projCoords * 0.5 + 0.5;
    if (projCoords.z > 1.0) return 0.0;
    float closestDepth = texture(shadowMap, projCoords.xy).r;
    float currentDepth = projCoords.z;
    float bias = 0.005;
    return currentDepth - bias > closestDepth ? 0.7 : 0.0;
}

void main() {
    vec4 albedoData = texture(gAlbedo, fragUV);
    vec4 normalData = texture(gNormal, fragUV);
    vec4 posData = texture(gPosition, fragUV);

    vec3 albedo = albedoData.rgb;
    float metallic = albedoData.a;
    vec3 N = normalize(normalData.rgb * 2.0 - 1.0);
    float roughness = normalData.a;
    vec3 worldPos = posData.rgb;
    float ao = posData.a;

    vec3 V = normalize(light.cameraPos.xyz - worldPos);
    vec3 L = normalize(-light.lightDir.xyz);
    vec3 H = normalize(V + L);

    vec3 F0 = mix(vec3(0.04), albedo, metallic);

    float NDF = DistributionGGX(N, H, roughness);
    float G   = GeometrySmith(N, V, L, roughness);
    vec3  F   = FresnelSchlick(max(dot(H, V), 0.0), F0);

    vec3 numerator = NDF * G * F;
    float denominator = 4.0 * max(dot(N, V), 0.0) * max(dot(N, L), 0.0) + 0.0001;
    vec3 specular = numerator / denominator;

    vec3 kD = (vec3(1.0) - F) * (1.0 - metallic);
    float NdotL = max(dot(N, L), 0.0);

    // Shadow
    vec4 fragPosLight = light.lightViewProj * vec4(worldPos, 1.0);
    float shadow = ShadowCalculation(fragPosLight);

    vec3 Lo = (kD * albedo / PI + specular) * light.lightColor.rgb * NdotL * (1.0 - shadow);
    vec3 ambient = light.ambientColor.rgb * albedo * ao;
    vec3 color = ambient + Lo;

    // Tone mapping (ACES)
    color = color / (color + vec3(1.0));
    // Gamma correction
    color = pow(color, vec3(1.0 / 2.2));

    outColor = vec4(color, 1.0);
}
)",
            "main", ShaderSource::Stage::Fragment};
  }

  // =====================================================================
  // COMPUTE SHADERS
  // =====================================================================

  static ShaderSource MorphTargetCompute() {
    return {"morph_compute.comp", R"(
#version 450

layout(local_size_x = 256) in;

struct Vertex {
    vec4 position;
    vec4 normal;
};

layout(std430, set = 0, binding = 0) readonly buffer BaseVertices {
    Vertex baseVerts[];
};

layout(std430, set = 0, binding = 1) readonly buffer TargetVertices {
    Vertex targetVerts[];
};

layout(std430, set = 0, binding = 2) writeonly buffer OutputVertices {
    Vertex outputVerts[];
};

layout(push_constant) uniform PushConstants {
    uint vertexCount;
    float weight;
    float pad1, pad2;
} pc;

void main() {
    uint idx = gl_GlobalInvocationID.x;
    if (idx >= pc.vertexCount) return;

    vec4 basePos = baseVerts[idx].position;
    vec4 targetPos = targetVerts[idx].position;
    vec4 baseNorm = baseVerts[idx].normal;
    vec4 targetNorm = targetVerts[idx].normal;

    outputVerts[idx].position = mix(basePos, targetPos, pc.weight);
    outputVerts[idx].normal = vec4(normalize(mix(baseNorm.xyz, targetNorm.xyz, pc.weight)), 0.0);
}
)",
            "main", ShaderSource::Stage::Compute};
  }

  static ShaderSource ShadowMapVertex() {
    return {"shadow.vert", R"(
#version 450

layout(set = 0, binding = 0) uniform LightUBO {
    mat4 lightViewProj;
} light;

layout(set = 1, binding = 0) uniform ObjectUBO {
    mat4 model;
} object;

layout(location = 0) in vec3 inPosition;

void main() {
    gl_Position = light.lightViewProj * object.model * vec4(inPosition, 1.0);
}
)",
            "main", ShaderSource::Stage::Vertex};
  }

  static ShaderSource FullscreenQuadVertex() {
    return {"fullscreen.vert", R"(
#version 450

layout(location = 0) out vec2 fragUV;

void main() {
    fragUV = vec2((gl_VertexIndex << 1) & 2, gl_VertexIndex & 2);
    gl_Position = vec4(fragUV * 2.0 - 1.0, 0.0, 1.0);
}
)",
            "main", ShaderSource::Stage::Vertex};
  }

  // Get all shaders for batch compilation
  static std::vector<ShaderSource> GetAllShaders() {
    return {GBufferVertex(),      GBufferFragment(), DeferredLightingFragment(),
            MorphTargetCompute(), ShadowMapVertex(), FullscreenQuadVertex()};
  }

  // Print shader manifest
  static void PrintManifest() {
    auto shaders = GetAllShaders();
    std::cout << "[ShaderLibrary] " << shaders.size()
              << " shaders registered:" << std::endl;
    for (const auto &s : shaders) {
      const char *stageStr = "?";
      switch (s.stage) {
      case ShaderSource::Stage::Vertex:
        stageStr = "VERT";
        break;
      case ShaderSource::Stage::Fragment:
        stageStr = "FRAG";
        break;
      case ShaderSource::Stage::Compute:
        stageStr = "COMP";
        break;
      case ShaderSource::Stage::Geometry:
        stageStr = "GEOM";
        break;
      default:
        break;
      }
      std::cout << "  [" << stageStr << "] " << s.name << " (" << s.glsl.size()
                << " bytes)" << std::endl;
    }
  }
};

} // namespace Graphics
} // namespace Mesozoic
