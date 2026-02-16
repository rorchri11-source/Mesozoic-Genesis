#pragma once
#include "../Core/Math/Vec3.h"
#include "UberMesh.h"
#include <cmath>
#include <vector>

using namespace Mesozoic::Math;

namespace Mesozoic {
namespace Graphics {

class TerrainGenerator {
public:
  // Simple deterministic hash for noise
  static float Hash(float n) {
    return std::fmod(std::sin(n) * 43758.5453123f, 1.0f);
  }

  static float Noise(float x, float z) {
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

  static float GetHeight(float x, float z) {
    // Large scale noise for biomes (plains vs mountains)
    float biome = Noise(x * 0.005f, z * 0.005f);

    // Base plains
    float h = 5.0f;

    // Smooth hills
    float hills = (Noise(x * 0.02f, z * 0.02f) * 0.5f +
                   Noise(x * 0.05f, z * 0.05f) * 0.25f) *
                  8.0f;

    // Large steep mountains
    float mtNoise = Noise(x * 0.008f, z * 0.008f);
    float mountains = std::pow(mtNoise, 2.5f) * 120.0f; // Much higher for snow

    // Blend based on biome
    if (biome < 0.4f) {
      // Pure plains
      h = 1.0f + Noise(x * 0.1f, z * 0.1f) * 0.5f;
    } else if (biome < 0.7f) {
      // Hills
      float t = (biome - 0.4f) / 0.3f;
      h = 1.0f + hills * t;
    } else {
      // Mountains
      float t = (biome - 0.7f) / 0.3f;
      h = 1.0f + hills + mountains * t;
    }

    return h;
  }

  static UberMesh GenerateGrid(int width, int depth, float scale) {
    UberMesh mesh;
    int vertexCount = (width + 1) * (depth + 1);
    mesh.baseVertices.resize(vertexCount);

    // Vertices
    for (int z = 0; z <= depth; z++) {
      for (int x = 0; x <= width; x++) {
        int index = z * (width + 1) + x;

        float posX = (float)x * scale - (width * scale * 0.5f);
        float posZ = (float)z * scale - (depth * scale * 0.5f);

        float posY = GetHeight(posX, posZ);

        mesh.baseVertices[index].position = {posX, posY, posZ};

        // Numerical normal (more accurate for complex noise)
        float eps = 0.5f;
        float hL = GetHeight(posX - eps, posZ);
        float hR = GetHeight(posX + eps, posZ);
        float hD = GetHeight(posX, posZ - eps);
        float hU = GetHeight(posX, posZ + eps);

        Vec3 normal = Vec3(hL - hR, 2.0f * eps, hD - hU).Normalized();

        mesh.baseVertices[index].normal = {normal.x, normal.y, normal.z};
        mesh.baseVertices[index].uv = {(float)x * 0.2f, (float)z * 0.2f};
        mesh.baseVertices[index].tangent = {1, 0, 0, 1};
        mesh.baseVertices[index].boneIndices = {0, 0, 0, 0};
        mesh.baseVertices[index].boneWeights = {1, 0, 0, 0};
      }
    }

    // Indices
    int indexCount = width * depth * 6;
    mesh.indices.resize(indexCount);

    int ptr = 0;
    for (int z = 0; z < depth; z++) {
      for (int x = 0; x < width; x++) {
        int topLeft = z * (width + 1) + x;
        int topRight = topLeft + 1;
        int bottomLeft = (z + 1) * (width + 1) + x;
        int bottomRight = bottomLeft + 1;

        mesh.indices[ptr++] = topLeft;
        mesh.indices[ptr++] = bottomLeft;
        mesh.indices[ptr++] = topRight;

        mesh.indices[ptr++] = topRight;
        mesh.indices[ptr++] = bottomLeft;
        mesh.indices[ptr++] = bottomRight;
      }
    }

    return mesh;
  }
};

} // namespace Graphics
} // namespace Mesozoic
