#pragma once
#include <cstdint>

#if defined(__cplusplus)
#include <array>
#include <iostream>
#include <vector>
using float2 = std::array<float, 2>;
using float3 = std::array<float, 3>;
using float4 = std::array<float, 4>;
using uint4 = std::array<uint32_t, 4>;
#else
// HLSL/GLSL compatible types would go here if we were processing shaders
#endif

namespace Mesozoic {
namespace Graphics {

// Standard PBR Vertex
struct Vertex {
  float3 position;
  float3 normal;
  float4 tangent; // w for handedness
  float2 uv;
  uint4 boneIndices;
  float4 boneWeights;
};

// Morph Target Delta
struct MorphDelta {
  float3 positionDelta;
  float3 normalDelta;
  // Tangents might be recalculated or morphed too
};

struct UberMesh {
  std::vector<Vertex> baseVertices;
  std::vector<uint32_t> indices;

  // Morph Targets: Each target has a list of deltas matching vertex count (or
  // sparse) For 10,000 entities, we use GPU Compute to apply these. But here is
  // the CPU data structure.
  struct Target {
    std::string name;
    std::vector<MorphDelta>
        deltas; // Same size as baseVertices for simplicity here
  };

  std::vector<Target> morphTargets;

  void AddMorphTarget(const std::string &name) {
    Target t;
    t.name = name;
    t.deltas.resize(baseVertices.size(), {{0, 0, 0}, {0, 0, 0}});
    morphTargets.push_back(t);
  }
};

} // namespace Graphics
} // namespace Mesozoic
