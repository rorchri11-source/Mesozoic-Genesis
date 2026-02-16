#pragma once
#include "../../Core/Math/Vec3.h"
#include <algorithm>
#include <cmath>
#include <vector>

namespace Mesozoic {
namespace Physics {

using Math::Quat;
using Math::Vec3;

struct IKJoint {
  Vec3 position;
  Quat rotation;
  float minAngle = -3.14159f; // Joint limits (radians)
  float maxAngle = 3.14159f;
};

class CCDSolver {
public:
  // Solves IK chain using Cyclic Coordinate Descent
  // joints[0] = root, joints[N-1] = end effector
  // Returns true if target was reached within tolerance
  static bool Solve(std::vector<IKJoint> &joints, const Vec3 &target,
                    int iterations = 15, float tolerance = 0.01f) {
    if (joints.size() < 2)
      return false;

    for (int iter = 0; iter < iterations; ++iter) {
      // Check convergence
      float distSq = Vec3::DistanceSq(joints.back().position, target);
      if (distSq < tolerance * tolerance)
        return true;

      // Iterate from second-to-last joint back to root
      for (int j = static_cast<int>(joints.size()) - 2; j >= 0; --j) {
        Vec3 &jointPos = joints[j].position;
        Vec3 &effectorPos = joints.back().position;

        // Vector from current joint to end effector
        Vec3 toEffector = (effectorPos - jointPos).Normalized();
        // Vector from current joint to target
        Vec3 toTarget = (target - jointPos).Normalized();

        // Skip if vectors are nearly parallel
        float dot = toEffector.Dot(toTarget);
        dot = std::clamp(dot, -1.0f, 1.0f);
        if (dot > 0.9999f)
          continue;

        // Calculate rotation axis and angle
        Vec3 axis = toEffector.Cross(toTarget);
        float axisLen = axis.Length();
        if (axisLen < 0.0001f)
          continue;
        axis = axis / axisLen;

        float angle = std::acos(dot);

        // Clamp to joint limits
        angle = std::clamp(angle, joints[j].minAngle, joints[j].maxAngle);

        // Create rotation quaternion
        Quat rotation = Quat::FromAxisAngle(axis, angle);
        joints[j].rotation = rotation * joints[j].rotation;

        // Rotate all downstream joints
        for (size_t k = static_cast<size_t>(j) + 1; k < joints.size(); ++k) {
          Vec3 offset = joints[k].position - jointPos;
          joints[k].position = jointPos + rotation.Rotate(offset);
          joints[k].rotation = rotation * joints[k].rotation;
        }
      }
    }

    // Return whether we're close enough
    return Vec3::DistanceSq(joints.back().position, target) <
           tolerance * tolerance;
  }

  // FABRIK-style pass for additional smoothing (optional)
  static void FABRIKBackward(std::vector<IKJoint> &joints, const Vec3 &target) {
    if (joints.size() < 2)
      return;

    // Store bone lengths
    std::vector<float> lengths(joints.size() - 1);
    for (size_t i = 0; i < joints.size() - 1; ++i) {
      lengths[i] = Vec3::Distance(joints[i].position, joints[i + 1].position);
    }

    // Backward pass: set end effector to target, adjust parents
    joints.back().position = target;
    for (int i = static_cast<int>(joints.size()) - 2; i >= 0; --i) {
      Vec3 dir = (joints[i].position - joints[i + 1].position).Normalized();
      joints[i].position = joints[i + 1].position + dir * lengths[i];
    }
  }
};

} // namespace Physics
} // namespace Mesozoic
