#pragma once
#include "../Genetics/DNA.h"
#include "UberMesh.h"
#include <algorithm>
#include <cmath>
#include <vector>

namespace Mesozoic {
namespace Graphics {

class MorphingSystem {
public:
  // CPU Reference implementation of the Vertex Shader logic
  // V_final = V_base + Sum(V_delta_i * Weight_i)
  static void ApplyMorphs(const UberMesh &mesh,
                          const std::vector<float> &weights,
                          std::vector<Vertex> &outVertices) {
    if (outVertices.size() != mesh.baseVertices.size()) {
      outVertices.resize(mesh.baseVertices.size());
    }

    // For each vertex
    for (size_t v = 0; v < mesh.baseVertices.size(); ++v) {
      // Start with base
      Vertex finalVert = mesh.baseVertices[v];

      // Accumulate deltas
      for (size_t t = 0; t < mesh.morphTargets.size(); ++t) {
        if (t >= weights.size())
          break;

        float w = weights[t];
        if (std::abs(w) < 0.001f)
          continue; // Optimization

        const auto &delta = mesh.morphTargets[t].deltas[v];

        finalVert.position[0] += delta.positionDelta[0] * w;
        finalVert.position[1] += delta.positionDelta[1] * w;
        finalVert.position[2] += delta.positionDelta[2] * w;

        finalVert.normal[0] += delta.normalDelta[0] * w;
        finalVert.normal[1] += delta.normalDelta[1] * w;
        finalVert.normal[2] += delta.normalDelta[2] * w;
      }
      outVertices[v] = finalVert;
    }
  }

  // Maps DNA Alleles to Morph Weights
  // E.g. Gene 0 (Size) -> Morph Target "Gigantism"
  static std::vector<float> DecodeDNA(const Mesozoic::Genetics::Genome &dna,
                                      size_t targetCount) {
    std::vector<float> weights(targetCount, 0.0f);

    // Example Mapping:
    // Locus 0: Size scale -> Target 0
    // Locus 1: Snout Length -> Target 1

    // 128 bit genome = 64 loci.
    for (size_t i = 0; i < std::min((size_t)64, targetCount); ++i) {
      uint8_t allele = dna.GetLocus((uint8_t)i);
      // ResolvePhenotype returns 0.0 to 1.5
      weights[i] = Mesozoic::Genetics::GeneticsEngine::ResolvePhenotype(allele);
    }

    return weights;
  }
};

} // namespace Graphics
} // namespace Mesozoic
