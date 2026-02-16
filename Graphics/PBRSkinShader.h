#pragma once
#include <array>
#include <cmath>
#include <cstdint>
#include <vector>


namespace Mesozoic {
namespace Graphics {

// PBR Material parameters for dinosaur skin
struct PBRMaterial {
  std::array<float, 3> albedo;
  float roughness;
  float metallic;
  float ao; // ambient occlusion

  // Subsurface Scattering (SSS)
  float sssStrength; // 0-1, how much light penetrates
  std::array<float, 3>
      sssColor;    // Color tint for scattered light (reddish for flesh)
  float sssRadius; // Scatter distance in world units

  // Texture Bombing parameters for micro-scales
  float scalePatternSize;     // Size of each scale tile
  float scalePatternContrast; // How visible scales are
  int scalePatternSeed;       // Randomization seed
};

// Dynamic wound/scar system
struct ScarData {
  std::array<float, 3> worldPosition;
  float radius;
  float depth;        // 0.0 = surface scratch, 1.0 = deep wound
  float healProgress; // 0.0 = fresh, 1.0 = fully healed scar
  float age;          // Time since inflicted (seconds)
};

class SkinShaderSystem {
public:
  // Per-entity skin data
  struct EntitySkin {
    uint32_t entityId;
    PBRMaterial material;
    std::vector<ScarData> scars;
  };

  std::vector<EntitySkin> skins;

  // Register an entity's skin
  void RegisterSkin(uint32_t entityId, const PBRMaterial &mat) {
    EntitySkin skin;
    skin.entityId = entityId;
    skin.material = mat;
    skins.push_back(skin);
  }

  // Add a wound/scar to an entity
  void InflictDamage(uint32_t entityId, const std::array<float, 3> &pos,
                     float radius, float depth) {
    for (auto &skin : skins) {
      if (skin.entityId == entityId) {
        ScarData scar;
        scar.worldPosition = pos;
        scar.radius = radius;
        scar.depth = depth;
        scar.healProgress = 0.0f;
        scar.age = 0.0f;
        skin.scars.push_back(scar);
        return;
      }
    }
  }

  // Update scar healing over time
  void UpdateScars(float dt) {
    for (auto &skin : skins) {
      for (auto &scar : skin.scars) {
        scar.age += dt;
        // Heal rate: takes ~300 seconds to fully heal
        scar.healProgress = std::min(1.0f, scar.age / 300.0f);
      }
    }
  }

  // Fragment Shader pseudocode for PBR + SSS + Scars:
  //
  // vec3 N = normalize(normal);
  // vec3 V = normalize(cameraPos - fragPos);
  // vec3 L = normalize(lightPos - fragPos);
  //
  // // Base PBR (Cook-Torrance)
  // float NdotL = max(dot(N, L), 0.0);
  // vec3 diffuse = albedo / PI;
  // vec3 specular = cookTorranceBRDF(N, V, L, roughness, metallic);
  // vec3 directLight = (diffuse + specular) * lightColor * NdotL;
  //
  // // SSS: light wrapping around thin surfaces
  // float sssWrap = max(0, dot(N, -L) + sssStrength) / (1.0 + sssStrength);
  // vec3 sssContrib = sssColor * sssWrap * thickness;
  //
  // // Scar mask: darken and roughen areas with scars
  // float scarMask = sampleScarTexture(fragPos, scars);
  // albedo = mix(albedo, scarColor, scarMask * (1.0 - healProgress));
  // roughness = mix(roughness, 0.9, scarMask);
  //
  // // Texture Bombing: tile micro-scale pattern
  // vec2 scaleUV = textureBomb(uv, scaleSize, scaleSeed);
  // vec3 scaleNormal = texture(scaleNormalMap, scaleUV).xyz;
  // N = perturbNormal(N, scaleNormal);
};

} // namespace Graphics
} // namespace Mesozoic
