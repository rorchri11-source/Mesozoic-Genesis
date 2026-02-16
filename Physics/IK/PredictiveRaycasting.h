#pragma once
#include "../../Core/Math/Vec3.h"
#include <cmath>

namespace Mesozoic {
namespace Physics {

using Math::Vec3;

struct Ray {
  Vec3 origin;
  Vec3 direction;
};

struct RaycastHit {
  bool hit;
  Vec3 position;
  Vec3 normal;
  float distance;
};

class PredictiveRaycasting {
public:
  // Projects a ray downward to find the terrain height at a future position
  static RaycastHit GetTerrainHeight(const Vec3 &futurePos) {
    Ray ray;
    ray.origin = Vec3(futurePos.x, futurePos.y + 10.0f, futurePos.z);
    ray.direction = Vec3(0.0f, -1.0f, 0.0f);

    // Mock collision logic with a simple ground plane or heightmap
    // Real implementation would interface with Physics Engine (e.g. Jolt)
    RaycastHit hit;
    hit.hit = true;
    hit.position = Vec3(futurePos.x, 0.0f, futurePos.z);
    hit.normal = Vec3(0.0f, 1.0f, 0.0f);
    hit.distance = futurePos.y + 10.0f;

    return hit;
  }

  // Predicts the spine tilt (pitch) based on terrain slope
  static float CalculateSpineTilt(const Vec3 &frontPos, const Vec3 &backPos) {
    RaycastHit frontHit = GetTerrainHeight(frontPos);
    RaycastHit backHit = GetTerrainHeight(backPos);

    if (frontHit.hit && backHit.hit) {
      float dy = frontHit.position.y - backHit.position.y;
      float dz = frontHit.position.z - backHit.position.z;
      return std::atan2(dy, std::abs(dz));
    }
    return 0.0f;
  }
};

} // namespace Physics
} // namespace Mesozoic
