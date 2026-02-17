#pragma once
#include "../Core/Math/Vec3.h"
<<<<<<< HEAD
#include "VulkanBackend.h"
=======
#include "VulkanBackend.h" // Needed for GPUTexture and backend pointer
>>>>>>> origin/main
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>


using namespace Mesozoic::Math;

namespace Mesozoic {
namespace Graphics {

class TerrainSystem {
public:
  VulkanBackend *backend = nullptr;

  // Config
  int width;
  int depth;
  float scale;
  float maxHeight;

  // Baked Data
  std::vector<float> heightMap;  // R32F
  std::vector<uint8_t> splatMap; // RGBA8

  // GPU Data
<<<<<<< HEAD
  VulkanBackend* backend = nullptr;
  GPUTexture heightTex;
  GPUTexture splatTex;

  // Constructor
  TerrainSystem() : width(512), depth(512), scale(3.0f), maxHeight(100.0f) {}
  TerrainSystem(int width, int depth, float scale, float maxHeight);

  // Initialization & GPU Upload
  void Initialize(VulkanBackend* backend);
=======
  GPUTexture heightMapTexture;
  GPUTexture splatMapTexture;

  // Constructor
  TerrainSystem() : width(512), depth(512), scale(1.0f), maxHeight(50.0f) {}
  TerrainSystem(int width, int depth, float scale, float maxHeight);

  void Initialize(VulkanBackend *backend);
>>>>>>> origin/main

  // Core Funcs
  void Bake();
  float GetHeight(float x, float z) const;
  Vec3 GetNormal(float x, float z) const;

  // Editor Funcs
  bool Raycast(Vec3 origin, Vec3 dir, Vec3 &hitPos);
  void Paint(float x, float z, float radius, int type); // type: 0=Grass, 1=Dirt, 2=Rock

  // Helper: Texture Dimensions
  int GetTextureWidth() const { return width; }
  int GetTextureHeight() const { return depth; }

private:
  void BakeHeightMap();
  void BakeSplatMap();
  void CreateTerrainTextures();

  // Internal Noise Helpers (Private Implementation)
  static float Hash(float n);
  static float Noise(float x, float z);
  static float GetHeightProcedural(float x, float z);
};

} // namespace Graphics
} // namespace Mesozoic
