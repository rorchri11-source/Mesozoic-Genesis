#pragma once
#include <array>
#include <bitset>
#include <cstdint>

namespace Mesozoic {
namespace Genetics {

// DNA 2.0: 128-bit compact genome
// Each gene (Locus) uses 2 bits: 1 for Paternal allele, 1 for Maternal allele.
// 0 = Recessive, 1 = Dominant (simplified model).
// Total 64 loci available.

struct Genome {
  std::bitset<128> data;

  uint8_t GetLocus(uint8_t index) const {
    if (index >= 64)
      return 0;
    uint8_t allele1 = data[index * 2] ? 1 : 0;
    uint8_t allele2 = data[index * 2 + 1] ? 1 : 0;
    return (allele1 << 1) | allele2;
  }

  void SetLocus(uint8_t index, bool paternal, bool maternal) {
    if (index >= 64)
      return;
    data[index * 2] = paternal;
    data[index * 2 + 1] = maternal;
  }
};

class GeneticsEngine {
public:
  // Phenotype Lookup Table
  static float ResolvePhenotype(uint8_t locusValue) {
    switch (locusValue) {
    case 0:
      return 0.2f; // rr
    case 1:
      return 1.0f; // rR
    case 2:
      return 1.0f; // Rr
    case 3:
      return 1.5f; // RR
    default:
      return 0.0f;
    }
  }

  // XorShift RNG - fast, non-zero period
  static uint32_t XorShift(uint32_t &seed) {
    seed ^= seed << 13;
    seed ^= seed >> 17;
    seed ^= seed << 5;
    // Ensure seed never becomes zero (XorShift has zero as absorbing state)
    if (seed == 0)
      seed = 1;
    return seed;
  }

  // Crossover: create child genome from two parents
  // seed: unique per breeding event, ensures diverse offspring
  static Genome Crossover(const Genome &father, const Genome &mother,
                          uint32_t &seed) {
    Genome child;
    for (int i = 0; i < 64; ++i) {
      // Father's contribution: pick allele 0 or 1
      uint32_t pick1 = XorShift(seed) % 2;
      bool pGene = father.data[static_cast<size_t>(i * 2 + pick1)];
      // Mother's contribution: pick allele 0 or 1
      uint32_t pick2 = XorShift(seed) % 2;
      bool mGene = mother.data[static_cast<size_t>(i * 2 + pick2)];
      child.SetLocus(static_cast<uint8_t>(i), pGene, mGene);
    }
    Mutate(child, seed);
    return child;
  }

  // Backwards-compatible overload (uses entity count as seed)
  static Genome Crossover(const Genome &father, const Genome &mother) {
    uint32_t seed = 12345u;
    return Crossover(father, mother, seed);
  }

  // Mutation: 0.1% chance of bit flip per bit
  static void Mutate(Genome &genome, uint32_t &seed) {
    for (size_t i = 0; i < 128; ++i) {
      uint32_t r = XorShift(seed);
      if ((r % 1000) == 0) { // 0.1%
        genome.data.flip(i);
      }
    }
  }
};

} // namespace Genetics
} // namespace Mesozoic
