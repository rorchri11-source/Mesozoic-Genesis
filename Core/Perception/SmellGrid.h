#pragma once
#include "../Math/Vec3.h"
#include <algorithm>
#include <cmath>
#include <vector>

namespace Mesozoic {
namespace Core {
namespace Perception {

using Math::Vec3;

// 3D Voxel Grid for Smell Simulation
// Uses Diffusion-Advection: scent spreads through air and is carried by wind
// Double-buffered, heap-allocated grids
class SmellGrid {
public:
  static constexpr int GRID_SIZE = 32;
  static constexpr float CELL_SIZE = 5.0f;
  static constexpr float DIFFUSION_RATE = 0.15f;
  static constexpr float DECAY_RATE = 0.02f;
  static constexpr int TOTAL_CELLS = GRID_SIZE * GRID_SIZE * GRID_SIZE;

  SmellGrid()
      : gridA(TOTAL_CELLS, 0.0f), gridB(TOTAL_CELLS, 0.0f), useA(true) {}

  float &At(std::vector<float> &grid, int x, int y, int z) {
    return grid[x + y * GRID_SIZE + z * GRID_SIZE * GRID_SIZE];
  }

  float At(const std::vector<float> &grid, int x, int y, int z) const {
    return grid[x + y * GRID_SIZE + z * GRID_SIZE * GRID_SIZE];
  }

  std::vector<float> &CurrentGrid() { return useA ? gridA : gridB; }
  std::vector<float> &BackGrid() { return useA ? gridB : gridA; }
  const std::vector<float> &CurrentGrid() const { return useA ? gridA : gridB; }

  void WorldToGrid(const Vec3 &worldPos, int &gx, int &gy, int &gz) const {
    float halfWorld = GRID_SIZE * CELL_SIZE * 0.5f;
    gx = std::clamp(static_cast<int>((worldPos.x + halfWorld) / CELL_SIZE), 0,
                    GRID_SIZE - 1);
    gy = std::clamp(static_cast<int>((worldPos.y) / CELL_SIZE), 0,
                    GRID_SIZE - 1);
    gz = std::clamp(static_cast<int>((worldPos.z + halfWorld) / CELL_SIZE), 0,
                    GRID_SIZE - 1);
  }

  void EmitScent(const Vec3 &worldPos, float amount) {
    int gx, gy, gz;
    WorldToGrid(worldPos, gx, gy, gz);
    auto &grid = CurrentGrid();

    for (int dx = -1; dx <= 1; dx++) {
      for (int dy = -1; dy <= 1; dy++) {
        for (int dz = -1; dz <= 1; dz++) {
          int nx = gx + dx, ny = gy + dy, nz = gz + dz;
          if (nx >= 0 && nx < GRID_SIZE && ny >= 0 && ny < GRID_SIZE &&
              nz >= 0 && nz < GRID_SIZE) {
            float falloff = (dx == 0 && dy == 0 && dz == 0) ? 1.0f : 0.3f;
            At(grid, nx, ny, nz) += amount * falloff;
          }
        }
      }
    }
  }

  void EmitScent(const std::array<float, 3> &pos, float amount) {
    EmitScent(Vec3(pos), amount);
  }

  void Update(float dt, const std::array<float, 3> &windArr) {
    Vec3 wind(windArr);
    auto &src = CurrentGrid();
    auto &dst = BackGrid();

    static const int dx[] = {1, -1, 0, 0, 0, 0};
    static const int dy[] = {0, 0, 1, -1, 0, 0};
    static const int dz[] = {0, 0, 0, 0, 1, -1};

    for (int x = 0; x < GRID_SIZE; x++) {
      for (int y = 0; y < GRID_SIZE; y++) {
        for (int z = 0; z < GRID_SIZE; z++) {
          float current = At(src, x, y, z);

          // Diffusion
          float neighborSum = 0.0f;
          int neighborCount = 0;
          for (int d = 0; d < 6; d++) {
            int nx = x + dx[d], ny = y + dy[d], nz = z + dz[d];
            if (nx >= 0 && nx < GRID_SIZE && ny >= 0 && ny < GRID_SIZE &&
                nz >= 0 && nz < GRID_SIZE) {
              neighborSum += At(src, nx, ny, nz);
              neighborCount++;
            }
          }
          float avgNeighbor =
              neighborCount > 0 ? neighborSum / neighborCount : 0.0f;
          float diffused =
              current + DIFFUSION_RATE * dt * (avgNeighbor - current);

          // Semi-Lagrangian advection
          int sx =
              std::clamp(static_cast<int>(static_cast<float>(x) - wind.x * dt),
                         0, GRID_SIZE - 1);
          int sy =
              std::clamp(static_cast<int>(static_cast<float>(y) - wind.y * dt),
                         0, GRID_SIZE - 1);
          int sz =
              std::clamp(static_cast<int>(static_cast<float>(z) - wind.z * dt),
                         0, GRID_SIZE - 1);
          float advected = At(src, sx, sy, sz);

          float windStrength = wind.Length();
          float advectWeight = std::min(1.0f, windStrength * 0.3f);
          float result =
              diffused * (1.0f - advectWeight) + advected * advectWeight;

          // Decay
          result *= (1.0f - DECAY_RATE * dt);
          result = std::max(0.0f, result);

          At(dst, x, y, z) = result;
        }
      }
    }
    useA = !useA;
  }

  float GetConcentration(const Vec3 &worldPos) const {
    int gx, gy, gz;
    const_cast<SmellGrid *>(this)->WorldToGrid(worldPos, gx, gy, gz);
    return At(CurrentGrid(), gx, gy, gz);
  }

  Vec3 GetGradient(const Vec3 &worldPos) const {
    int gx, gy, gz;
    const_cast<SmellGrid *>(this)->WorldToGrid(worldPos, gx, gy, gz);
    auto &grid = CurrentGrid();

    Vec3 grad;
    if (gx > 0 && gx < GRID_SIZE - 1)
      grad.x = At(grid, gx + 1, gy, gz) - At(grid, gx - 1, gy, gz);
    if (gy > 0 && gy < GRID_SIZE - 1)
      grad.y = At(grid, gx, gy + 1, gz) - At(grid, gx, gy - 1, gz);
    if (gz > 0 && gz < GRID_SIZE - 1)
      grad.z = At(grid, gx, gy, gz + 1) - At(grid, gx, gy, gz - 1);

    return grad.Normalized();
  }

private:
  std::vector<float> gridA;
  std::vector<float> gridB;
  bool useA;
};

} // namespace Perception
} // namespace Core
} // namespace Mesozoic
