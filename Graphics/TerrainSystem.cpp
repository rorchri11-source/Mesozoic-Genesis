#include "TerrainSystem.h"
#include <iostream>

namespace Mesozoic {
namespace Graphics {

TerrainSystem::TerrainSystem(int w, int d, float s, float mh)
    : width(w), depth(d), scale(s), maxHeight(mh) {}

<<<<<<< HEAD
void TerrainSystem::Initialize(VulkanBackend *backend) {
  this->backend = backend;
  Bake();

  // Upload HeightMap
  heightTex = backend->CreateTextureFromBuffer(
      heightMap.data(), heightMap.size() * sizeof(float), width, depth,
      VK_FORMAT_R32_SFLOAT);

  // Upload SplatMap
  splatTex = backend->CreateTextureFromBuffer(
      splatMap.data(), splatMap.size(), width, depth,
      VK_FORMAT_R8G8B8A8_UNORM); // Ensure UNORM for correct color/weight interpretation

  // Update Global Terrain Descriptors
  backend->UpdateDescriptorSets(heightTex, splatTex);
=======
void TerrainSystem::Initialize(VulkanBackend *b) {
  std::cout << "[TerrainSystem] Initializing..." << std::endl;
  backend = b;
  Bake();
  CreateTerrainTextures();
>>>>>>> origin/main
}

void TerrainSystem::Bake() {
  std::cout << "[TerrainSystem] Baking HeightMap..." << std::endl;
  BakeHeightMap();
  std::cout << "[TerrainSystem] Baking SplatMap..." << std::endl;
  BakeSplatMap();
}

float TerrainSystem::Hash(float n) {
  // Exact C++ fmod behavior for consistency
  return std::abs(std::fmod(std::sin(n) * 43758.5453123f, 1.0f));
}

float TerrainSystem::Noise(float x, float z) {
  float ix = std::floor(x);
  float iz = std::floor(z);
  float fx = std::fmod(x, 1.0f);
  if (fx < 0)
    fx += 1.0f;
  float fz = std::fmod(z, 1.0f);
  if (fz < 0)
    fz += 1.0f;

  // Quintic interpolation
  float ux = fx * fx * fx * (fx * (fx * 6.0f - 15.0f) + 10.0f);
  float uz = fz * fz * fz * (fz * (fz * 6.0f - 15.0f) + 10.0f);

  auto h = [](float x, float z) {
    float n = x + z * 57.0f;
    return std::abs(std::fmod(std::sin(n) * 43758.5453123f, 1.0f));
  };

  float a = h(ix, iz);
  float b = h(ix + 1, iz);
  float c = h(ix, iz + 1);
  float d = h(ix + 1, iz + 1);

  return a + (b - a) * ux + (c - a) * uz + (a - b - c + d) * ux * uz;
}

float TerrainSystem::GetHeightProcedural(float x, float z) {
  // Matches the Shader Logic 1:1
  float biome = Noise(x * 0.005f, z * 0.005f);

  // Base plains
  float h = 5.0f;

  // Smooth hills
  float hills = (Noise(x * 0.02f, z * 0.02f) * 0.5f +
                 Noise(x * 0.05f, z * 0.05f) * 0.25f) *
                8.0f;

  // Large steep mountains
  float mtNoise = Noise(x * 0.008f, z * 0.008f);
  float mountains = std::pow(mtNoise, 2.5f) * 120.0f;

  // Blend based on biome
  if (biome < 0.4f) {
    h = 1.0f + Noise(x * 0.1f, z * 0.1f) * 0.5f;
  } else if (biome < 0.7f) {
    float t = (biome - 0.4f) / 0.3f;
    h = 1.0f + hills * t;
  } else {
    float t = (biome - 0.7f) / 0.3f;
    h = 1.0f + hills + mountains * t;
  }
  return h;
}

void TerrainSystem::BakeHeightMap() {
  heightMap.resize(width * depth);
  float halfWidth = (width * scale) * 0.5f;
  float halfDepth = (depth * scale) * 0.5f;

  for (int z = 0; z < depth; ++z) {
    for (int x = 0; x < width; ++x) {
      float worldX = (x * scale) - halfWidth;
      float worldZ = (z * scale) - halfDepth;
      heightMap[z * width + x] = GetHeightProcedural(worldX, worldZ);
    }
  }
}

void TerrainSystem::BakeSplatMap() {
  splatMap.resize(width * depth * 4); // RGBA
  float halfWidth = (width * scale) * 0.5f;
  float halfDepth = (depth * scale) * 0.5f;

  for (int z = 0; z < depth; ++z) {
    for (int x = 0; x < width; ++x) {
      int idx = (z * width + x) * 4;

      float worldX = (x * scale) - halfWidth;
      float worldZ = (z * scale) - halfDepth;
      float h = GetHeightProcedural(worldX, worldZ);

      // Calculate Slope
      float eps = 1.0f;
      float hL = GetHeightProcedural(worldX - eps, worldZ);
      float hR = GetHeightProcedural(worldX + eps, worldZ);
      float slope = std::abs(hL - hR) / (2.0f * eps); // Simple derivative

      // Logic:
      // High Slope (> 0.6) -> Rock (Blue)
      // Low Height (< 3.0) -> Dirt (Green)
      // Else -> Grass (Red)

      uint8_t r = 0, g = 0, b = 0, a = 255; // Alpha for AO (todo)

      if (slope > 0.8f) {
        b = 255; // Pure Rock on cliffs
      } else if (slope > 0.4f) {
        // Blend Grass/Rock
        float t = (slope - 0.4f) / 0.4f;
        r = (uint8_t)((1.0f - t) * 255);
        b = (uint8_t)(t * 255);
      } else {
        if (h < 4.0f) {
          g = 255; // Dirt in lowlands/shores
        } else {
          r = 255; // Lush Grass
        }
      }

      splatMap[idx + 0] = r;
      splatMap[idx + 1] = g;
      splatMap[idx + 2] = b;
      splatMap[idx + 3] = a;
    }
  }
}

void TerrainSystem::CreateTerrainTextures() {
  if (!backend)
    return;

  // HeightMap (R32F)
  heightMapTexture = backend->CreateTextureFromBuffer(
      heightMap.data(), heightMap.size() * sizeof(float), width, depth,
      VK_FORMAT_R32_SFLOAT);

  // SplatMap (RGBA8)
  splatMapTexture = backend->CreateTextureFromBuffer(
      splatMap.data(), splatMap.size(), width, depth, VK_FORMAT_R8G8B8A8_UNORM);

  backend->UpdateDescriptorSets(heightMapTexture, splatMapTexture);
}

float TerrainSystem::GetHeight(float x, float z) const {
  float halfWidth = (width * scale) * 0.5f;
  float halfDepth = (depth * scale) * 0.5f;

  // Convert world space to texture space (0..width-1)
  float u = (x + halfWidth) / scale;
  float v = (z + halfDepth) / scale;

  // Clamp
  if (u < 0)
    u = 0;
  if (v < 0)
    v = 0;
  if (u >= width - 1)
    u = width - 1.001f;
  if (v >= depth - 1)
    v = depth - 1.001f;

  // Bilinear Interpolation
  int x0 = (int)u;
  int z0 = (int)v;
  int x1 = x0 + 1;
  int z1 = z0 + 1;

  float tx = u - x0;
  float tz = v - z0;

  float h00 = heightMap[z0 * width + x0];
  float h10 = heightMap[z0 * width + x1];
  float h01 = heightMap[z1 * width + x0];
  float h11 = heightMap[z1 * width + x1];

  float h0 = h00 * (1 - tx) + h10 * tx;
  float h1 = h01 * (1 - tx) + h11 * tx;

  return h0 * (1 - tz) + h1 * tz;
}

Vec3 TerrainSystem::GetNormal(float x, float z) const {
  float eps = 0.5f;
  float hL = GetHeight(x - eps, z);
  float hR = GetHeight(x + eps, z);
  float hD = GetHeight(x, z - eps);
  float hU = GetHeight(x, z + eps);

  return Vec3(hL - hR, 2.0f * eps, hD - hU).Normalized();
}

bool TerrainSystem::Raycast(Vec3 origin, Vec3 dir, Vec3 &hitPos) {
  // Simple stepping DDA-like approach
  Vec3 p = origin;
  Vec3 step = dir.Normalized() * (scale * 0.5f); // Half grid step resolution

  // Max distance
  float maxDist = 1000.0f;
  float dist = 0.0f;

  while (dist < maxDist) {
    p = p + step;
    dist += step.Length();

    // Bounds check
    float halfWidth = (width * scale) * 0.5f;
    float halfDepth = (depth * scale) * 0.5f;
    if (p.x < -halfWidth || p.x > halfWidth || p.z < -halfDepth ||
        p.z > halfDepth) {
      continue;
    }

    float h = GetHeight(p.x, p.z);
    if (p.y <= h) {
      hitPos = p;
      return true;
    }
  }
  return false;
}

void TerrainSystem::Paint(float x, float z, float radius, int type) {
  // x, z are world coords. Convert to texture coords.
  float halfWidth = (width * scale) * 0.5f;
  float halfDepth = (depth * scale) * 0.5f;

  int cx = (int)((x + halfWidth) / scale);
  int cz = (int)((z + halfDepth) / scale);

  int r = (int)(radius / scale);

  bool dirty = false;

  for (int j = -r; j <= r; ++j) {
    for (int i = -r; i <= r; ++i) {
      int tx = cx + i;
      int tz = cz + j;

      if (tx < 0 || tx >= width || tz < 0 || tz >= depth)
        continue;
      if (i * i + j * j > r * r)
        continue;

      int idx = (tz * width + tx) * 4;

      // Simple paint: set channel based on type
      // type 0: Grass (Red), 1: Dirt (Green), 2: Rock (Blue)
      // We could do blending, but hard replacement is simpler for "Paint"
      if (type == 0) {
        splatMap[idx + 0] = 255;
        splatMap[idx + 1] = 0;
        splatMap[idx + 2] = 0;
      } else if (type == 1) {
        splatMap[idx + 0] = 0;
        splatMap[idx + 1] = 255;
        splatMap[idx + 2] = 0;
      } else if (type == 2) {
        splatMap[idx + 0] = 0;
        splatMap[idx + 1] = 0;
        splatMap[idx + 2] = 255;
      }
      dirty = true;
    }
  }

  // Upload if changed
  if (dirty && backend && splatMapTexture.IsValid()) {
    backend->UpdateTexture(splatMapTexture, splatMap.data(), splatMap.size());
  }
}

} // namespace Graphics
} // namespace Mesozoic
