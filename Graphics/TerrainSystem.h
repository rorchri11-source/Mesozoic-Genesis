#pragma once
#include "../Core/Math/Vec3.h"
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

  // Constructor
  TerrainSystem(int width, int depth, float scale, float maxHeight);

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
