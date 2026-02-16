#pragma once
#include "../Core/Math/Vec3.h"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <vector>

namespace Mesozoic {
namespace Physics {

using Math::Vec3;

// =========================================================================
// Terrain Heightmap System
// =========================================================================

struct TerrainConfig {
  uint32_t resolution = 256; // Grid resolution (NxN)
  float worldSize = 512.0f;  // World units covered
  float maxHeight = 50.0f;   // Maximum terrain height
  float waterLevel = 5.0f;   // Water surface height
};

class TerrainHeightmap {
  TerrainConfig config;
  std::vector<float> heightData;  // Raw height values [0..1]
  std::vector<Vec3> normalData;   // Per-vertex normals
  std::vector<uint8_t> biomeData; // Biome type per cell

public:
  enum class Biome : uint8_t {
    Water = 0,
    Beach = 1,
    Grassland = 2,
    Forest = 3,
    Rocky = 4,
    Mountain = 5
  };

  void Initialize(const TerrainConfig &cfg) {
    config = cfg;
    uint32_t n = config.resolution;
    heightData.resize(n * n);
    normalData.resize(n * n);
    biomeData.resize(n * n);

    // Generate with multi-octave Perlin noise (simplified)
    GenerateHeightmap();
    ComputeNormals();
    ClassifyBiomes();

    std::cout << "[Terrain] Initialized " << n << "x" << n << " heightmap ("
              << config.worldSize << "m world)" << std::endl;
  }

  // Get terrain height at world position (bilinear interpolation)
  float GetHeight(float worldX, float worldZ) const {
    float u = (worldX + config.worldSize * 0.5f) / config.worldSize;
    float v = (worldZ + config.worldSize * 0.5f) / config.worldSize;

    u = std::clamp(u, 0.0f, 1.0f);
    v = std::clamp(v, 0.0f, 1.0f);

    float fx = u * (config.resolution - 1);
    float fy = v * (config.resolution - 1);
    int x0 = static_cast<int>(fx);
    int y0 = static_cast<int>(fy);
    int x1 = std::min(x0 + 1, static_cast<int>(config.resolution - 1));
    int y1 = std::min(y0 + 1, static_cast<int>(config.resolution - 1));
    float tx = fx - x0;
    float ty = fy - y0;

    float h00 = heightData[y0 * config.resolution + x0];
    float h10 = heightData[y0 * config.resolution + x1];
    float h01 = heightData[y1 * config.resolution + x0];
    float h11 = heightData[y1 * config.resolution + x1];

    float h = h00 * (1 - tx) * (1 - ty) + h10 * tx * (1 - ty) +
              h01 * (1 - tx) * ty + h11 * tx * ty;

    return h * config.maxHeight;
  }

  // Get terrain normal at world position
  Vec3 GetNormal(float worldX, float worldZ) const {
    float cellSize = config.worldSize / config.resolution;
    float hL = GetHeight(worldX - cellSize, worldZ);
    float hR = GetHeight(worldX + cellSize, worldZ);
    float hD = GetHeight(worldX, worldZ - cellSize);
    float hU = GetHeight(worldX, worldZ + cellSize);
    return Vec3(hL - hR, 2.0f * cellSize, hD - hU).Normalized();
  }

  // Get terrain slope angle (radians) at position
  float GetSlope(float worldX, float worldZ) const {
    Vec3 normal = GetNormal(worldX, worldZ);
    return std::acos(std::clamp(normal.y, -1.0f, 1.0f));
  }

  // Get biome at position
  Biome GetBiome(float worldX, float worldZ) const {
    float u = (worldX + config.worldSize * 0.5f) / config.worldSize;
    float v = (worldZ + config.worldSize * 0.5f) / config.worldSize;
    int x =
        static_cast<int>(std::clamp(u, 0.0f, 1.0f) * (config.resolution - 1));
    int y =
        static_cast<int>(std::clamp(v, 0.0f, 1.0f) * (config.resolution - 1));
    return static_cast<Biome>(biomeData[y * config.resolution + x]);
  }

  bool IsUnderwater(float worldX, float worldZ) const {
    return GetHeight(worldX, worldZ) < config.waterLevel;
  }

  float GetWaterLevel() const { return config.waterLevel; }
  const TerrainConfig &GetConfig() const { return config; }
  const std::vector<float> &GetRawHeights() const { return heightData; }
  const std::vector<Vec3> &GetNormals() const { return normalData; }

  // Find a random valid spawn position (above water, not too steep)
  Vec3 FindSpawnPosition(uint32_t seed) const {
    const int maxAttempts = 100;
    for (int i = 0; i < maxAttempts; i++) {
      // XorShift RNG
      seed ^= seed << 13;
      seed ^= seed >> 17;
      seed ^= seed << 5;
      if (seed == 0)
        seed = 1;

      float x = (static_cast<float>(seed % 10000) / 10000.0f - 0.5f) *
                config.worldSize * 0.8f;
      seed ^= seed << 13;
      seed ^= seed >> 17;
      seed ^= seed << 5;
      float z = (static_cast<float>(seed % 10000) / 10000.0f - 0.5f) *
                config.worldSize * 0.8f;

      float h = GetHeight(x, z);
      float slope = GetSlope(x, z);

      if (h > config.waterLevel + 1.0f && slope < 0.6f) {
        return Vec3(x, h, z);
      }
    }
    return Vec3(0, GetHeight(0, 0), 0);
  }

  // Export to RAW format for external tools
  bool ExportRAW(const std::string &filepath) const {
    std::ofstream file(filepath, std::ios::binary);
    if (!file.is_open())
      return false;

    for (float h : heightData) {
      uint16_t val =
          static_cast<uint16_t>(std::clamp(h, 0.0f, 1.0f) * 65535.0f);
      file.write(reinterpret_cast<const char *>(&val), 2);
    }
    std::cout << "[Terrain] Exported RAW to " << filepath << std::endl;
    return true;
  }

private:
  // Simple value noise (seeded, deterministic)
  static float ValueNoise(float x, float y, uint32_t seed) {
    int ix = static_cast<int>(std::floor(x));
    int iy = static_cast<int>(std::floor(y));
    float fx = x - ix;
    float fy = y - iy;
    fx = fx * fx * (3.0f - 2.0f * fx); // smoothstep
    fy = fy * fy * (3.0f - 2.0f * fy);

    auto hash = [seed](int x, int y) -> float {
      uint32_t h = seed;
      h ^= static_cast<uint32_t>(x) * 374761393u;
      h ^= static_cast<uint32_t>(y) * 668265263u;
      h = (h ^ (h >> 13)) * 1274126177u;
      h ^= h >> 16;
      return static_cast<float>(h & 0xFFFF) / 65535.0f;
    };

    float v00 = hash(ix, iy);
    float v10 = hash(ix + 1, iy);
    float v01 = hash(ix, iy + 1);
    float v11 = hash(ix + 1, iy + 1);

    float top = v00 + (v10 - v00) * fx;
    float bot = v01 + (v11 - v01) * fx;
    return top + (bot - top) * fy;
  }

  void GenerateHeightmap() {
    uint32_t n = config.resolution;
    uint32_t seed = 42;

    for (uint32_t y = 0; y < n; y++) {
      for (uint32_t x = 0; x < n; x++) {
        float fx = static_cast<float>(x) / n;
        float fy = static_cast<float>(y) / n;

        // Multi-octave noise
        float height = 0.0f;
        float amplitude = 0.5f;
        float frequency = 2.0f;
        for (int octave = 0; octave < 6; octave++) {
          height += ValueNoise(fx * frequency, fy * frequency, seed + octave) *
                    amplitude;
          frequency *= 2.0f;
          amplitude *= 0.5f;
        }

        // Island shape: fade edges to zero
        float cx = fx - 0.5f;
        float cy = fy - 0.5f;
        float edgeDist = 1.0f - std::sqrt(cx * cx + cy * cy) * 2.2f;
        edgeDist = std::clamp(edgeDist, 0.0f, 1.0f);
        height *= edgeDist * edgeDist;

        heightData[y * n + x] = std::clamp(height, 0.0f, 1.0f);
      }
    }
  }

  void ComputeNormals() {
    uint32_t n = config.resolution;
    for (uint32_t y = 0; y < n; y++) {
      for (uint32_t x = 0; x < n; x++) {
        float hL = (x > 0) ? heightData[y * n + x - 1] : heightData[y * n + x];
        float hR =
            (x < n - 1) ? heightData[y * n + x + 1] : heightData[y * n + x];
        float hD =
            (y > 0) ? heightData[(y - 1) * n + x] : heightData[y * n + x];
        float hU =
            (y < n - 1) ? heightData[(y + 1) * n + x] : heightData[y * n + x];

        normalData[y * n + x] =
            Vec3((hL - hR) * config.maxHeight, 2.0f * config.worldSize / n,
                 (hD - hU) * config.maxHeight)
                .Normalized();
      }
    }
  }

  void ClassifyBiomes() {
    uint32_t n = config.resolution;
    float waterNorm = config.waterLevel / config.maxHeight;

    for (uint32_t i = 0; i < n * n; i++) {
      float h = heightData[i];
      float slope = std::acos(std::clamp(normalData[i].y, -1.0f, 1.0f));

      if (h < waterNorm)
        biomeData[i] = static_cast<uint8_t>(Biome::Water);
      else if (h < waterNorm + 0.02f)
        biomeData[i] = static_cast<uint8_t>(Biome::Beach);
      else if (slope > 0.7f)
        biomeData[i] = static_cast<uint8_t>(Biome::Rocky);
      else if (h > 0.65f)
        biomeData[i] = static_cast<uint8_t>(Biome::Mountain);
      else if (h > 0.35f)
        biomeData[i] = static_cast<uint8_t>(Biome::Forest);
      else
        biomeData[i] = static_cast<uint8_t>(Biome::Grassland);
    }
  }
};

} // namespace Physics
} // namespace Mesozoic
