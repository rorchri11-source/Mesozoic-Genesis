#pragma once
#include "../../Core/Math/Vec3.h"
#include "CCDSolver.h"
#include "PredictiveRaycasting.h"
#include <algorithm>
#include <cmath>

namespace Mesozoic {
namespace Physics {

using Math::Vec3;

// Spine Pitching: tilts the dinosaur's spine based on terrain slope
// to maintain a realistic center of gravity on uneven terrain.
class SpinePitching {
public:
  struct SpineConfig {
    float maxPitchAngle; // Max forward/backward tilt (radians)
    float maxRollAngle;  // Max side tilt (radians)
    float smoothingRate; // How fast the spine adjusts (0-1 per frame)
    float spineLength;   // Distance from front legs to back legs
  };

  struct SpineState {
    float currentPitch;
    float currentRoll;
    Vec3 frontFootTarget;
    Vec3 backFootTarget;
  };

  static SpineState CalculateSpine(const Vec3 &entityPos,
                                   const Vec3 &entityForward,
                                   const SpineConfig &config,
                                   const SpineState &prevState, float dt) {
    SpineState state = prevState;

    // Project front and back foot positions
    Vec3 frontPos = entityPos + entityForward * (config.spineLength * 0.5f);
    Vec3 backPos = entityPos - entityForward * (config.spineLength * 0.5f);

    // Raycast terrain height at front and back
    RaycastHit frontHit = PredictiveRaycasting::GetTerrainHeight(frontPos);
    RaycastHit backHit = PredictiveRaycasting::GetTerrainHeight(backPos);

    if (frontHit.hit && backHit.hit) {
      state.frontFootTarget = frontHit.position;
      state.backFootTarget = backHit.position;

      // Calculate target pitch
      float dy = frontHit.position.y - backHit.position.y;
      float targetPitch = std::atan2(dy, config.spineLength);
      targetPitch =
          std::clamp(targetPitch, -config.maxPitchAngle, config.maxPitchAngle);

      // Smooth interpolation (framerate-independent)
      float t = 1.0f - std::pow(1.0f - config.smoothingRate, dt * 60.0f);
      state.currentPitch += (targetPitch - state.currentPitch) * t;
    }

    return state;
  }
};

} // namespace Physics
} // namespace Mesozoic
