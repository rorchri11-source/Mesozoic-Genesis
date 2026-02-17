#pragma once
#include <array>
#include <cmath>

namespace Mesozoic {
namespace Math {

// =========================================================================
// Vec3: 3D vector used across all engine modules
// =========================================================================
struct Vec3 {
  float x, y, z;

  Vec3() : x(0), y(0), z(0) {}
  Vec3(float x, float y, float z) : x(x), y(y), z(z) {}
  explicit Vec3(const std::array<float, 3> &a) : x(a[0]), y(a[1]), z(a[2]) {}

  std::array<float, 3> ToArray() const { return {x, y, z}; }

  Vec3 operator+(const Vec3 &o) const { return {x + o.x, y + o.y, z + o.z}; }
  Vec3 operator-(const Vec3 &o) const { return {x - o.x, y - o.y, z - o.z}; }
  Vec3 operator*(float s) const { return {x * s, y * s, z * s}; }
  Vec3 operator/(float s) const {
    float inv = 1.0f / s;
    return {x * inv, y * inv, z * inv};
  }
  Vec3 &operator+=(const Vec3 &o) {
    x += o.x;
    y += o.y;
    z += o.z;
    return *this;
  }
  Vec3 &operator-=(const Vec3 &o) {
    x -= o.x;
    y -= o.y;
    z -= o.z;
    return *this;
  }
  Vec3 &operator*=(float s) {
    x *= s;
    y *= s;
    z *= s;
    return *this;
  }
  Vec3 operator-() const { return {-x, -y, -z}; }

  float Dot(const Vec3 &o) const { return x * o.x + y * o.y + z * o.z; }

  Vec3 Cross(const Vec3 &o) const {
    return {y * o.z - z * o.y, z * o.x - x * o.z, x * o.y - y * o.x};
  }

  float LengthSq() const { return x * x + y * y + z * z; }
  float Length() const { return std::sqrt(LengthSq()); }

  Vec3 Normalized() const {
    float len = Length();
    if (len < 0.00001f)
      return {0, 0, 0};
    return *this / len;
  }

  static Vec3 Lerp(const Vec3 &a, const Vec3 &b, float t) {
    return a * (1.0f - t) + b * t;
  }

  static float Distance(const Vec3 &a, const Vec3 &b) {
    return (a - b).Length();
  }

  static float DistanceSq(const Vec3 &a, const Vec3 &b) {
    return (a - b).LengthSq();
  }
};

inline Vec3 operator*(float s, const Vec3 &v) { return v * s; }

// =========================================================================
// Quaternion: rotation representation
// =========================================================================
struct Quat {
  float x, y, z, w;

  Quat() : x(0), y(0), z(0), w(1) {}
  Quat(float x, float y, float z, float w) : x(x), y(y), z(z), w(w) {}

  static Quat Identity() { return {0, 0, 0, 1}; }

  static Quat FromAxisAngle(const Vec3 &axis, float angleRad) {
    float halfAngle = angleRad * 0.5f;
    float s = std::sin(halfAngle);
    Vec3 n = axis.Normalized();
    return {n.x * s, n.y * s, n.z * s, std::cos(halfAngle)};
  }

  Quat Normalized() const {
    float len = std::sqrt(x * x + y * y + z * z + w * w);
    if (len < 0.00001f)
      return Identity();
    float inv = 1.0f / len;
    return {x * inv, y * inv, z * inv, w * inv};
  }

  Quat operator*(const Quat &q) const {
    return {w * q.x + x * q.w + y * q.z - z * q.y,
            w * q.y - x * q.z + y * q.w + z * q.x,
            w * q.z + x * q.y - y * q.x + z * q.w,
            w * q.w - x * q.x - y * q.y - z * q.z};
  }

  Vec3 Rotate(const Vec3 &v) const {
    Vec3 qv(x, y, z);
    Vec3 uv = qv.Cross(v);
    Vec3 uuv = qv.Cross(uv);
    return v + (uv * w + uuv) * 2.0f;
  }

  static Quat Slerp(const Quat &a, const Quat &b, float t) {
    float dot = a.x * b.x + a.y * b.y + a.z * b.z + a.w * b.w;
    Quat target = b;
    if (dot < 0.0f) { // Take shortest path
      target = {-b.x, -b.y, -b.z, -b.w};
      dot = -dot;
    }
    if (dot > 0.9995f) { // Linear interpolation for near-identical quaternions
      return Quat{a.x + t * (target.x - a.x), a.y + t * (target.y - a.y),
                  a.z + t * (target.z - a.z), a.w + t * (target.w - a.w)}
          .Normalized();
    }
    float theta = std::acos(dot);
    float sinTheta = std::sin(theta);
    float wa = std::sin((1.0f - t) * theta) / sinTheta;
    float wb = std::sin(t * theta) / sinTheta;
    return {wa * a.x + wb * target.x, wa * a.y + wb * target.y,
            wa * a.z + wb * target.z, wa * a.w + wb * target.w};
  }
};

// =========================================================================
// Mat4: 4x4 column-major matrix
// =========================================================================
struct Mat4 {
  float m[16]; // Column-major: m[col*4 + row]

  Mat4() { Identity(); }

  void Identity() {
    for (int i = 0; i < 16; i++)
      m[i] = 0;
    m[0] = m[5] = m[10] = m[15] = 1.0f;
  }

  float &operator()(int row, int col) { return m[col * 4 + row]; }
  float operator()(int row, int col) const { return m[col * 4 + row]; }

  Mat4 operator*(const Mat4 &o) const {
    Mat4 result;
    for (int i = 0; i < 16; i++)
      result.m[i] = 0;
    for (int col = 0; col < 4; col++) {
      for (int row = 0; row < 4; row++) {
        for (int k = 0; k < 4; k++) {
          result.m[col * 4 + row] += m[k * 4 + row] * o.m[col * 4 + k];
        }
      }
    }
    return result;
  }

  Vec3 TransformPoint(const Vec3 &v) const {
    float w = m[3] * v.x + m[7] * v.y + m[11] * v.z + m[15];
    if (std::abs(w) < 0.00001f)
      w = 1.0f;
    return {(m[0] * v.x + m[4] * v.y + m[8] * v.z + m[12]) / w,
            (m[1] * v.x + m[5] * v.y + m[9] * v.z + m[13]) / w,
            (m[2] * v.x + m[6] * v.y + m[10] * v.z + m[14]) / w};
  }

  Vec3 TransformDirection(const Vec3 &v) const {
    return {m[0] * v.x + m[4] * v.y + m[8] * v.z,
            m[1] * v.x + m[5] * v.y + m[9] * v.z,
            m[2] * v.x + m[6] * v.y + m[10] * v.z};
  }

  static Mat4 Translation(const Vec3 &t) {
    Mat4 r;
    r.m[12] = t.x;
    r.m[13] = t.y;
    r.m[14] = t.z;
    return r;
  }

  static Mat4 Scale(const Vec3 &s) {
    Mat4 r;
    r.m[0] = s.x;
    r.m[5] = s.y;
    r.m[10] = s.z;
    return r;
  }

  static Mat4 FromQuat(const Quat &q) {
    Mat4 r;
    float xx = q.x * q.x, yy = q.y * q.y, zz = q.z * q.z;
    float xy = q.x * q.y, xz = q.x * q.z, yz = q.y * q.z;
    float wx = q.w * q.x, wy = q.w * q.y, wz = q.w * q.z;
    r.m[0] = 1 - 2 * (yy + zz);
    r.m[1] = 2 * (xy + wz);
    r.m[2] = 2 * (xz - wy);
    r.m[4] = 2 * (xy - wz);
    r.m[5] = 1 - 2 * (xx + zz);
    r.m[6] = 2 * (yz + wx);
    r.m[8] = 2 * (xz + wy);
    r.m[9] = 2 * (yz - wx);
    r.m[10] = 1 - 2 * (xx + yy);
    r.m[15] = 1;
    return r;
  }

  static Mat4 Perspective(float fovDeg, float aspect, float near, float far) {
    Mat4 r;
    for (int i = 0; i < 16; i++)
      r.m[i] = 0;
    float tanHalf = std::tan(fovDeg * 3.14159f / 360.0f);
    r.m[0] = 1.0f / (aspect * tanHalf);
    r.m[5] = 1.0f / tanHalf;
    r.m[10] = -(far + near) / (far - near);
    r.m[11] = -1.0f;
    r.m[14] = -(2.0f * far * near) / (far - near);
    return r;
  }

  static Mat4 LookAt(const Vec3 &eye, const Vec3 &target, const Vec3 &up) {
    Vec3 f = (target - eye).Normalized();
    Vec3 r = f.Cross(up).Normalized();
    Vec3 u = r.Cross(f);
    Mat4 result;
    result.m[0] = r.x;
    result.m[4] = r.y;
    result.m[8] = r.z;
    result.m[1] = u.x;
    result.m[5] = u.y;
    result.m[9] = u.z;
    result.m[2] = -f.x;
    result.m[6] = -f.y;
    result.m[10] = -f.z;
    result.m[12] = -r.Dot(eye);
    result.m[13] = -u.Dot(eye);
    result.m[14] = f.Dot(eye);
    result.m[15] = 1.0f;
    return result;
  }

  std::array<float, 16> ToArray() const {
    std::array<float, 16> a;
    for (int i = 0; i < 16; i++)
      a[i] = m[i];
    return a;
  }
};

} // namespace Math
} // namespace Mesozoic
