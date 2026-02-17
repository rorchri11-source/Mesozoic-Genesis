#pragma once
#include "../Core/Math/Vec3.h"
#include "VulkanBackend.h"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>


using namespace Mesozoic::Math;

namespace Mesozoic {
namespace Graphics {

class TerrainSystem {
public:
  // Config
  int width;
  int depth;
  float scale;
  float maxHeight;

  // Baked Data
  std::vector<float> heightMap;  // R32F
  std::vector<uint8_t> splatMap; // RGBA8

  // GPU Data
  VulkanBackend* backend = nullptr;
  GPUTexture heightTex;
  GPUTexture splatTex;

  // Constructor
  TerrainSystem() : width(512), depth(512), scale(3.0f), maxHeight(100.0f) {}
  TerrainSystem(int width, int depth, float scale, float maxHeight);

  // Initialization & GPU Upload
  void Initialize(VulkanBackend* backend);

  // Core Funcs
  void Bake();
  float GetHeight(float x, float z) const;
  Vec3 GetNormal(float x, float z) const;

  // Helper: Texture Dimensions
  int GetTextureWidth() const { return width; }
  int GetTextureHeight() const { return depth; }

private:
  void BakeHeightMap();
  void BakeSplatMap();

  // Internal Noise Helpers (Private Implementation)
  static float Hash(float n);
  static float Noise(float x, float z);
  static float GetHeightProcedural(float x, float z);
};

} // namespace Graphics
} // namespace Mesozoic
