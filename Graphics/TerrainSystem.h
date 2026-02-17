// Resolved Conflicts
#pragma once
#include "../Core/Math/Vec3.h"
#include "Renderer.h"
#include "TerrainGenerator.h"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

using namespace Mesozoic::Math;

namespace Mesozoic {
namespace Graphics {

class Renderer;

class TerrainSystem {
public:
  // Config
  int width = 512;
  int depth = 512;
  float scale = 3.0f;
  float maxHeight = 50.0f;

  // Baked Data
  std::vector<float> heightMap;  // R32F
  std::vector<uint8_t> splatMap; // RGBA8

  // GPU Resources
  GPUTexture heightMapTex;
  GPUTexture splatMapTex;
  uint32_t meshId = 0xFFFFFFFF;
  UberMesh mesh; // Keep CPU copy for updates
  Renderer *renderer = nullptr;

  // Constructor
  TerrainSystem() = default;
  TerrainSystem(int width, int depth, float scale, float maxHeight);

  void Initialize(Renderer *renderer, int width, int depth, float scale,
                  float maxHeight);

  // Core Funcs
  void Bake();
  float GetHeight(float x, float z) const;
  Vec3 GetNormal(float x, float z) const;

  // Editing
  bool Raycast(const Vec3 &origin, const Vec3 &dir, float &t,
               Vec3 &hitPoint) const;
  void ModifyHeight(float x, float z, float radius, float strength, int mode, float targetHeight = 0.0f);
  void UpdateMesh();     // Push CPU mesh changes to GPU
  void UpdateTextures(); // Push CPU texture changes to GPU

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
