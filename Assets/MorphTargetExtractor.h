#pragma once
#include "../Core/Math/Vec3.h"
#include "GLTFLoader.h"
#include <cmath>
#include <iostream>
#include <string>
#include <vector>


namespace Mesozoic {
namespace Assets {

using Math::Vec3;

// =========================================================================
// Morph Target Data
// =========================================================================

struct MorphTarget {
  std::string name;
  std::vector<Vec3> positionDeltas; // Difference from base mesh
  std::vector<Vec3> normalDeltas;
  float defaultWeight = 0.0f;
};

struct MorphTargetSet {
  std::string meshName;
  std::vector<MorphTarget> targets;
  std::vector<GLTFVertex> baseMesh; // Reference base mesh

  // Apply morph targets to base mesh with given weights
  std::vector<GLTFVertex> Apply(const std::vector<float> &weights) const {
    auto result = baseMesh;
    size_t vertCount = baseMesh.size();

    for (size_t t = 0; t < targets.size() && t < weights.size(); t++) {
      float w = weights[t];
      if (std::abs(w) < 0.001f)
        continue;

      const auto &target = targets[t];
      for (size_t v = 0; v < vertCount && v < target.positionDeltas.size();
           v++) {
        result[v].position = result[v].position + target.positionDeltas[v] * w;
        if (v < target.normalDeltas.size()) {
          result[v].normal =
              (result[v].normal + target.normalDeltas[v] * w).Normalized();
        }
      }
    }
    return result;
  }
};

// =========================================================================
// Morph Target Extractor
// =========================================================================

class MorphTargetExtractor {
public:
  // Extract morph targets from multiple mesh variants
  // baseMesh = default shape, variants = named shape keys
  static MorphTargetSet
  Extract(const GLTFMesh &baseMesh,
          const std::vector<std::pair<std::string, GLTFMesh>> &variants) {
    MorphTargetSet set;
    set.meshName = baseMesh.name;

    if (baseMesh.primitives.empty())
      return set;
    set.baseMesh = baseMesh.primitives[0].vertices;

    for (const auto &[name, variantMesh] : variants) {
      if (variantMesh.primitives.empty())
        continue;
      const auto &variantVerts = variantMesh.primitives[0].vertices;

      MorphTarget target;
      target.name = name;
      target.positionDeltas.resize(set.baseMesh.size());
      target.normalDeltas.resize(set.baseMesh.size());

      for (size_t v = 0; v < set.baseMesh.size() && v < variantVerts.size();
           v++) {
        target.positionDeltas[v] =
            variantVerts[v].position - set.baseMesh[v].position;
        target.normalDeltas[v] =
            variantVerts[v].normal - set.baseMesh[v].normal;
      }

      set.targets.push_back(target);
    }

    std::cout << "[MorphTargetExtractor] Extracted " << set.targets.size()
              << " morph targets from '" << set.meshName << "'" << std::endl;
    return set;
  }

  // Generate dinosaur growth morph targets (DNA-driven body scaling)
  static MorphTargetSet GenerateDinosaurMorphs(const GLTFMesh &baseMesh) {
    MorphTargetSet set;
    set.meshName = baseMesh.name;

    if (baseMesh.primitives.empty())
      return set;
    set.baseMesh = baseMesh.primitives[0].vertices;
    size_t vertCount = set.baseMesh.size();

    // Growth target: scale up uniformly
    {
      MorphTarget target;
      target.name = "growth";
      target.positionDeltas.resize(vertCount);
      target.normalDeltas.resize(vertCount, Vec3(0, 0, 0));
      for (size_t v = 0; v < vertCount; v++) {
        target.positionDeltas[v] = set.baseMesh[v].position * 0.5f; // +50% size
      }
      set.targets.push_back(target);
    }

    // Muscle mass: inflate laterally
    {
      MorphTarget target;
      target.name = "muscle";
      target.positionDeltas.resize(vertCount);
      target.normalDeltas.resize(vertCount, Vec3(0, 0, 0));
      for (size_t v = 0; v < vertCount; v++) {
        Vec3 outward = set.baseMesh[v].normal * 0.2f;
        outward.y *= 0.5f; // Less vertical inflation
        target.positionDeltas[v] = outward;
      }
      set.targets.push_back(target);
    }

    // Fat: expand belly
    {
      MorphTarget target;
      target.name = "fat";
      target.positionDeltas.resize(vertCount);
      target.normalDeltas.resize(vertCount, Vec3(0, 0, 0));
      for (size_t v = 0; v < vertCount; v++) {
        float belly = std::exp(-set.baseMesh[v].position.y *
                               set.baseMesh[v].position.y * 2.0f);
        Vec3 outward = set.baseMesh[v].normal * (0.15f * belly);
        target.positionDeltas[v] = outward;
      }
      set.targets.push_back(target);
    }

    // Elongate: stretch along Z axis
    {
      MorphTarget target;
      target.name = "elongate";
      target.positionDeltas.resize(vertCount);
      target.normalDeltas.resize(vertCount, Vec3(0, 0, 0));
      for (size_t v = 0; v < vertCount; v++) {
        target.positionDeltas[v] =
            Vec3(0, 0, set.baseMesh[v].position.z * 0.3f);
      }
      set.targets.push_back(target);
    }

    // Jaw size: scale head region
    {
      MorphTarget target;
      target.name = "jaw";
      target.positionDeltas.resize(vertCount);
      target.normalDeltas.resize(vertCount, Vec3(0, 0, 0));
      for (size_t v = 0; v < vertCount; v++) {
        float headRegion = std::max(0.0f, set.baseMesh[v].position.z - 1.0f);
        headRegion = std::min(headRegion, 1.0f);
        target.positionDeltas[v] =
            set.baseMesh[v].normal * (0.15f * headRegion);
      }
      set.targets.push_back(target);
    }

    // Horn/crest growth
    {
      MorphTarget target;
      target.name = "crest";
      target.positionDeltas.resize(vertCount);
      target.normalDeltas.resize(vertCount, Vec3(0, 0, 0));
      for (size_t v = 0; v < vertCount; v++) {
        float topHead = std::max(0.0f, set.baseMesh[v].position.y - 1.5f);
        float headZ = std::max(0.0f, set.baseMesh[v].position.z - 1.0f);
        float crestFactor = topHead * headZ;
        target.positionDeltas[v] =
            Vec3(0, crestFactor * 0.4f, crestFactor * 0.1f);
      }
      set.targets.push_back(target);
    }

    std::cout << "[MorphTargetExtractor] Generated " << set.targets.size()
              << " DNA-driven morph targets for '" << set.meshName << "'"
              << std::endl;
    return set;
  }
};

} // namespace Assets
} // namespace Mesozoic
