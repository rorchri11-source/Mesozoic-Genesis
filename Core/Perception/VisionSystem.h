#pragma once
#include "../Math/Vec3.h"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

namespace Mesozoic {
namespace Core {
namespace Perception {

using Math::Vec3;

struct EntityPerceptionData {
  uint32_t entityId;
  Vec3 position;
  float radius;
  bool isPredator;
  float stealthFactor; // 0 = fully visible, 1 = invisible
};

struct VisibleEntity {
  uint32_t entityId;
  float distance;
  float angle; // Angle from forward vector (radians)
  bool isPredator;
};

class VisionSystem {
public:
  float fovDegrees;
  float maxRange;
  float nightPenalty;

  VisionSystem() : fovDegrees(120.0f), maxRange(100.0f), nightPenalty(0.0f) {}
  VisionSystem(float fov, float range)
      : fovDegrees(fov), maxRange(range), nightPenalty(0.0f) {}

  std::vector<VisibleEntity>
  ProcessVision(const std::array<float, 3> &obsPosArr,
                const std::array<float, 3> &obsFwdArr,
                const std::vector<EntityPerceptionData> &entities,
                uint32_t observerId) const {
    Vec3 observerPos(obsPosArr);
    Vec3 observerForward(obsFwdArr);
    return ProcessVision(observerPos, observerForward, entities, observerId);
  }

  std::vector<VisibleEntity>
  ProcessVision(const Vec3 &observerPos, const Vec3 &observerForward,
                const std::vector<EntityPerceptionData> &entities,
                uint32_t observerId) const {
    std::vector<VisibleEntity> visible;
    float halfFovRad = (fovDegrees * 0.5f) * 3.14159f / 180.0f;
    float cosHalfFov = std::cos(halfFovRad);
    float effectiveRange = maxRange * (1.0f - nightPenalty * 0.6f);

    Vec3 fwdNorm = observerForward.Normalized();

    for (const auto &ent : entities) {
      if (ent.entityId == observerId)
        continue;

      Vec3 toEnt = ent.position - observerPos;
      float distSq = toEnt.LengthSq();

      float adjustedRange = effectiveRange + ent.radius;
      float adjustedRangeSq = adjustedRange * adjustedRange;

      if (distSq > adjustedRangeSq)
        continue;

      float stealthFactor = 1.0f - ent.stealthFactor * 0.8f;
      float stealthRange = adjustedRange * stealthFactor;
      float stealthRangeSq = stealthRange * stealthRange;

      if (distSq > stealthRangeSq && ent.stealthFactor > 0.5f)
        continue;

      float dist = std::sqrt(distSq);

      Vec3 dirNorm;
      if (dist > 0.00001f) {
        float invDist = 1.0f / dist;
        dirNorm = toEnt * invDist;
      } else {
        dirNorm = Vec3(0, 0, 0);
      }

      float dotProduct = fwdNorm.Dot(dirNorm);

      if (dotProduct >= cosHalfFov) {
        VisibleEntity ve;
        ve.entityId = ent.entityId;
        ve.distance = dist;
        ve.angle = std::acos(std::clamp(dotProduct, -1.0f, 1.0f));
        ve.isPredator = ent.isPredator;
        visible.push_back(ve);
      }
    }

    std::sort(visible.begin(), visible.end(),
              [](const VisibleEntity &a, const VisibleEntity &b) {
                return a.distance < b.distance;
              });

    return visible;
  }

  bool DetectThreat(const Vec3 &observerPos, const Vec3 &observerForward,
                    const std::vector<EntityPerceptionData> &entities,
                    uint32_t observerId, VisibleEntity &outThreat) const {
    auto visible =
        ProcessVision(observerPos, observerForward, entities, observerId);
    for (const auto &ve : visible) {
      if (ve.isPredator) {
        outThreat = ve;
        return true;
      }
    }
    return false;
  }
};

} // namespace Perception
} // namespace Core
} // namespace Mesozoic
