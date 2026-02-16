#pragma once
#include "Vec3.h"
#include <array>
#include <cmath>
#include <cstring>

namespace Mesozoic {
namespace Math {

struct Matrix4 {
  std::array<float, 16> m;

  static Matrix4 Identity() {
    Matrix4 res;
    res.m.fill(0);
    res.m[0] = 1;
    res.m[5] = 1;
    res.m[10] = 1;
    res.m[15] = 1;
    return res;
  }

  static Matrix4 Perspective(float fovRadians, float aspect, float zNear,
                             float zFar) {
    Matrix4 res;
    res.m.fill(0);
    float tanHalfFov = std::tan(fovRadians / 2.0f);
    res.m[0] = 1.0f / (aspect * tanHalfFov);
    res.m[5] =
        1.0f /
        tanHalfFov; // Vulkan Y is down? If so, negate this? GLM does -1.0f.
    // Standard GL projection maps Y to [-1, 1]. Vulkan Y is down.
    // So we might need -1.0f here to flip Y or flip in viewport.
    // Let's use standard GL for now and fix later.
    res.m[5] *= -1.0f; // Flip Y for Vulkan

    res.m[10] = zFar / (zNear - zFar);
    res.m[11] = -1.0f;
    res.m[14] = -(zFar * zNear) / (zFar - zNear);
    return res;
  }

  static Matrix4 LookAt(const Vec3 &eye, const Vec3 &center, const Vec3 &up) {
    Vec3 f = (center - eye).Normalized();
    Vec3 s = f.Cross(up).Normalized();
    Vec3 u = s.Cross(f);

    Matrix4 res = Identity();
    res.m[0] = s.x;
    res.m[4] = s.y;
    res.m[8] = s.z;
    res.m[1] = u.x;
    res.m[5] = u.y;
    res.m[9] = u.z;
    res.m[2] = -f.x;
    res.m[6] = -f.y;
    res.m[10] = -f.z;
    res.m[12] = -s.Dot(eye);
    res.m[13] = -u.Dot(eye);
    res.m[14] = f.Dot(eye);
    return res;
  }

  Matrix4 operator*(const Matrix4 &other) const {
    Matrix4 res;
    for (int row = 0; row < 4; ++row) {
      for (int col = 0; col < 4; ++col) {
        res.m[col * 4 + row] = m[0 * 4 + row] * other.m[col * 4 + 0] +
                               m[1 * 4 + row] * other.m[col * 4 + 1] +
                               m[2 * 4 + row] * other.m[col * 4 + 2] +
                               m[3 * 4 + row] * other.m[col * 4 + 3];
      }
    }
    return res;
  }

  // Multiply by array (assuming array is row-major or column-major matching
  // logic) Here we wrap array in Matrix4
};

} // namespace Math
} // namespace Mesozoic
