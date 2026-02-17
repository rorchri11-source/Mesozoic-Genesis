// Resolved Conflicts
#include "TerrainSystem.h"
#include "Renderer.h"
#include <iostream>

namespace Mesozoic {
namespace Graphics {

TerrainSystem::TerrainSystem(int w, int d, float s, float mh)
    : width(w), depth(d), scale(s), maxHeight(mh) {}

void TerrainSystem::Initialize(Renderer *r, int w, int d, float s, float mh) {
  renderer = r;
  width = w;
  depth = d;
  scale = s;
  maxHeight = mh;

  Bake();

  // Create GPU Textures
  // HeightMap: R32_SFLOAT
  heightMapTex = renderer->backend.CreateTextureFromBuffer(
      heightMap.data(), heightMap.size() * sizeof(float), width, depth,
      VK_FORMAT_R32_SFLOAT);

  // SplatMap: R8G8B8A8_UNORM
  splatMapTex = renderer->backend.CreateTextureFromBuffer(
      splatMap.data(), splatMap.size() * sizeof(uint8_t), width, depth,
      VK_FORMAT_R8G8B8A8_UNORM);

  // Update Global Descriptor Set
  renderer->backend.UpdateDescriptorSets(heightMapTex, splatMapTex);

  // Generate Mesh
  std::cout << "[TerrainSystem] Generating Mesh..." << std::endl;
  mesh = TerrainGenerator::GenerateGrid(width, depth, scale);

  // Apply Initial Heights to Mesh
  for (auto &v : mesh.baseVertices) {
    float h = GetHeight(v.position[0], v.position[2]);
    v.position[1] = h;
    Vec3 n = GetNormal(v.position[0], v.position[2]);
    v.normal = {n.x, n.y, n.z};
  }

  // Register Mesh
  meshId = renderer->RegisterMesh(mesh);
  std::cout << "[TerrainSystem] Initialized. Mesh ID: " << meshId << std::endl;
}

void TerrainSystem::Bake() {
  std::cout << "[TerrainSystem] Baking HeightMap..." << std::endl;
  BakeHeightMap();
  std::cout << "[TerrainSystem] Baking SplatMap..." << std::endl;
  BakeSplatMap();
}

float TerrainSystem::Hash(float n) {
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
  float biome = Noise(x * 0.005f, z * 0.005f);
  float h = 5.0f;
  float hills = (Noise(x * 0.02f, z * 0.02f) * 0.5f +
                 Noise(x * 0.05f, z * 0.05f) * 0.25f) *
                8.0f;
  float mtNoise = Noise(x * 0.008f, z * 0.008f);
  float mountains = std::pow(mtNoise, 2.5f) * 120.0f;

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
  splatMap.resize(width * depth * 4);
  float halfWidth = (width * scale) * 0.5f;
  float halfDepth = (depth * scale) * 0.5f;

  for (int z = 0; z < depth; ++z) {
    for (int x = 0; x < width; ++x) {
      int idx = (z * width + x) * 4;
      float worldX = (x * scale) - halfWidth;
      float worldZ = (z * scale) - halfDepth;
      float h = GetHeightProcedural(worldX, worldZ);

      float eps = 1.0f;
      float hL = GetHeightProcedural(worldX - eps, worldZ);
      float hR = GetHeightProcedural(worldX + eps, worldZ);
      float slope = std::abs(hL - hR) / (2.0f * eps);

      uint8_t r = 0, g = 0, b = 0, a = 255;
      if (slope > 0.8f) {
        b = 255;
      } else if (slope > 0.4f) {
        float t = (slope - 0.4f) / 0.4f;
        r = (uint8_t)((1.0f - t) * 255);
        b = (uint8_t)(t * 255);
      } else {
        if (h < 4.0f) {
          g = 255;
        } else {
          r = 255;
        }
      }

      splatMap[idx + 0] = r;
      splatMap[idx + 1] = g;
      splatMap[idx + 2] = b;
      splatMap[idx + 3] = a;
    }
  }
}

float TerrainSystem::GetHeight(float x, float z) const {
  float halfWidth = (width * scale) * 0.5f;
  float halfDepth = (depth * scale) * 0.5f;

  float u = (x + halfWidth) / scale;
  float v = (z + halfDepth) / scale;

  if (u < 0)
    u = 0;
  if (v < 0)
    v = 0;
  if (u >= width - 1)
    u = width - 1.001f;
  if (v >= depth - 1)
    v = depth - 1.001f;

  int x0 = (int)u;
  int z0 = (int)v;
  int x1 = x0 + 1;
  int z1 = z0 + 1;

  float tx = u - x0;
  float tz = v - z0;

  if (x0 < 0 || x0 >= width || z0 < 0 || z0 >= depth) return 0.0f;
  if (x1 >= width) x1 = width - 1;
  if (z1 >= depth) z1 = depth - 1;

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

bool TerrainSystem::Raycast(const Vec3 &origin, const Vec3 &dir, float &t,
                            Vec3 &hitPoint) const {
  float step = scale * 0.5f;
  float maxDist = 2000.0f;
  Vec3 currentPos = origin;
  float d = 0.0f;

  // Simple optimization: Start ray from top of bounding box?
  // Not strictly necessary for mouse picking which usually points down.

  while (d < maxDist) {
    currentPos = origin + dir * d;
    float h = GetHeight(currentPos.x, currentPos.z);

    // Check if under terrain
    // Note: GetHeight clamps to edges.
    if (currentPos.y < h) {
        // Binary search refinement could go here for precision
        t = d;
        hitPoint = currentPos;
        hitPoint.y = h;
        return true;
    }

    d += step;
  }
  return false;
}

void TerrainSystem::ModifyHeight(float worldX, float worldZ, float radius,
                                 float strength, int mode, float targetHeight) {
  // 0: Raise, 1: Lower, 2: Flatten (Average/Snap)

  float halfWidth = (width * scale) * 0.5f;
  float halfDepth = (depth * scale) * 0.5f;

  // Convert world center to grid coords
  float centerU = (worldX + halfWidth) / scale;
  float centerV = (worldZ + halfDepth) / scale;

  float radiusGrid = radius / scale;
  int minX = (int)(centerU - radiusGrid);
  int maxX = (int)(centerU + radiusGrid);
  int minZ = (int)(centerV - radiusGrid);
  int maxZ = (int)(centerV + radiusGrid);

  minX = std::max(0, minX);
  maxX = std::min(width - 1, maxX);
  minZ = std::max(0, minZ);
  maxZ = std::min(depth - 1, maxZ);

  // If mode == 2 (Flatten), targetHeight is passed in.
  // If not passed (default 0), main.cpp should have handled logic to pass the correct height.
  // Wait, if ModifyHeight is called from UI, main.cpp needs to pass it.

  bool dirty = false;
  for (int z = minZ; z <= maxZ; ++z) {
    for (int x = minX; x <= maxX; ++x) {
        float dx = (x - centerU);
        float dz = (z - centerV);
        float distSq = dx*dx + dz*dz;
        if (distSq < radiusGrid * radiusGrid) {
            float dist = std::sqrt(distSq);
            float falloff = 1.0f - (dist / radiusGrid);
            falloff = falloff * falloff; // smooth quadratic falloff

            int idx = z * width + x;
            float currentH = heightMap[idx];

            if (mode == 0) { // Raise
                heightMap[idx] += strength * falloff;
            } else if (mode == 1) { // Lower
                heightMap[idx] -= strength * falloff;
            } else if (mode == 2) { // Flatten
                // Lerp towards target
                heightMap[idx] += (targetHeight - currentH) * strength * falloff;
            }
            dirty = true;
        }
    }
  }

  if (dirty) {
      UpdateMesh();
      UpdateTextures();
  }
}

void TerrainSystem::UpdateMesh() {
  if (!renderer || meshId == 0xFFFFFFFF) return;

  // We only need to update Y positions and Normals in the mesh vertices
  // But UpdateMesh takes a full vector of Vertex.
  // So we update our CPU copy 'mesh' and send it.

  // Recalculate all vertices? Or just the ones we touched?
  // Renderer::UpdateMesh replaces the WHOLE buffer.
  // So we must update the whole CPU mesh.
  // Optimization: Keep CPU mesh up to date.

  float halfWidth = (width * scale) * 0.5f;
  float halfDepth = (depth * scale) * 0.5f;

  for (auto &v : mesh.baseVertices) {
    // Re-sample height from the updated heightMap
    float h = GetHeight(v.position[0], v.position[2]);
    v.position[1] = h;

    // Re-calculate normal
    Vec3 n = GetNormal(v.position[0], v.position[2]);
    v.normal = {n.x, n.y, n.z};
  }

  renderer->UpdateMesh(meshId, mesh.baseVertices);
}

void TerrainSystem::UpdateTextures() {
  if (!renderer) return;
  // Upload new heightMap data to GPU texture
  // Texture is R32_SFLOAT, heightMap is float[]. Matches.
  renderer->backend.UpdateTexture(heightMapTex, heightMap.data(), heightMap.size() * sizeof(float));

  // Note: Splat map update logic is not implemented in ModifyHeight yet,
  // but if we did painting, we would call:
  // renderer->backend.UpdateTexture(splatMapTex, splatMap.data(), splatMap.size() * sizeof(uint8_t));
}

} // namespace Graphics
} // namespace Mesozoic
