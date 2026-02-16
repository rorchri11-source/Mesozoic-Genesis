#pragma once
#include "../Core/Math/Vec3.h"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <vector>

namespace Mesozoic {
namespace Physics {

using Math::Vec3;

// =========================================================================
// Collision Primitives
// =========================================================================

struct AABB {
  Vec3 min, max;

  Vec3 Center() const { return (min + max) * 0.5f; }
  Vec3 Extents() const { return (max - min) * 0.5f; }

  bool Intersects(const AABB &other) const {
    return (min.x <= other.max.x && max.x >= other.min.x) &&
           (min.y <= other.max.y && max.y >= other.min.y) &&
           (min.z <= other.max.z && max.z >= other.min.z);
  }

  bool Contains(const Vec3 &point) const {
    return point.x >= min.x && point.x <= max.x && point.y >= min.y &&
           point.y <= max.y && point.z >= min.z && point.z <= max.z;
  }

  void Expand(const Vec3 &point) {
    min.x = std::min(min.x, point.x);
    min.y = std::min(min.y, point.y);
    min.z = std::min(min.z, point.z);
    max.x = std::max(max.x, point.x);
    max.y = std::max(max.y, point.y);
    max.z = std::max(max.z, point.z);
  }
};

struct Sphere {
  Vec3 center;
  float radius;

  bool Intersects(const Sphere &other) const {
    float distSq = (center - other.center).LengthSq();
    float rSum = radius + other.radius;
    return distSq <= rSum * rSum;
  }
};

struct Capsule {
  Vec3 a, b; // Line segment endpoints
  float radius;

  float Length() const { return (b - a).Length(); }
};

// =========================================================================
// Collision Result
// =========================================================================

struct CollisionResult {
  bool hit = false;
  Vec3 contactPoint;
  Vec3 contactNormal;
  float penetrationDepth = 0.0f;
  uint32_t entityA = 0;
  uint32_t entityB = 0;
};

// =========================================================================
// Collider Component (attached to entities)
// =========================================================================

enum class ColliderType : uint8_t { Sphere, AABB, Capsule };

struct Collider {
  ColliderType type = ColliderType::Sphere;
  Vec3 offset;
  union {
    struct {
      float radius;
    } sphere;
    struct {
      float halfExtents[3];
    } box;
    struct {
      float radius;
      float height;
    } capsule;
  };
  uint32_t entityId = 0;
  uint32_t layer = 0xFFFFFFFF; // Collision layer bitmask
  bool isTrigger = false;      // Trigger = no physics response, just events

  Collider() : sphere{1.0f} {}
};

// =========================================================================
// Spatial Hash Grid (broad-phase)
// =========================================================================

class SpatialHashGrid {
  struct Cell {
    std::vector<uint32_t> colliderIndices;
  };

  float cellSize;
  int gridSize;
  std::vector<Cell> cells;

  int Hash(int x, int y, int z) const {
    // Wrap coordinates
    x = ((x % gridSize) + gridSize) % gridSize;
    y = ((y % gridSize) + gridSize) % gridSize;
    z = ((z % gridSize) + gridSize) % gridSize;
    return x + y * gridSize + z * gridSize * gridSize;
  }

public:
  SpatialHashGrid(float cellSize = 10.0f, int gridSize = 64)
      : cellSize(cellSize), gridSize(gridSize) {
    cells.resize(static_cast<size_t>(gridSize * gridSize * gridSize));
  }

  void Clear() {
    for (auto &cell : cells)
      cell.colliderIndices.clear();
  }

  void Insert(uint32_t colliderIndex, const Vec3 &position, float radius) {
    int minX = static_cast<int>(std::floor((position.x - radius) / cellSize));
    int maxX = static_cast<int>(std::floor((position.x + radius) / cellSize));
    int minY = static_cast<int>(std::floor((position.y - radius) / cellSize));
    int maxY = static_cast<int>(std::floor((position.y + radius) / cellSize));
    int minZ = static_cast<int>(std::floor((position.z - radius) / cellSize));
    int maxZ = static_cast<int>(std::floor((position.z + radius) / cellSize));

    for (int x = minX; x <= maxX; x++)
      for (int y = minY; y <= maxY; y++)
        for (int z = minZ; z <= maxZ; z++)
          cells[Hash(x, y, z)].colliderIndices.push_back(colliderIndex);
  }

  std::vector<uint32_t> Query(const Vec3 &position, float radius) const {
    std::vector<uint32_t> result;
    int minX = static_cast<int>(std::floor((position.x - radius) / cellSize));
    int maxX = static_cast<int>(std::floor((position.x + radius) / cellSize));
    int minY = static_cast<int>(std::floor((position.y - radius) / cellSize));
    int maxY = static_cast<int>(std::floor((position.y + radius) / cellSize));
    int minZ = static_cast<int>(std::floor((position.z - radius) / cellSize));
    int maxZ = static_cast<int>(std::floor((position.z + radius) / cellSize));

    for (int x = minX; x <= maxX; x++)
      for (int y = minY; y <= maxY; y++)
        for (int z = minZ; z <= maxZ; z++) {
          const auto &cell = cells[Hash(x, y, z)];
          result.insert(result.end(), cell.colliderIndices.begin(),
                        cell.colliderIndices.end());
        }

    // Remove duplicates
    std::sort(result.begin(), result.end());
    result.erase(std::unique(result.begin(), result.end()), result.end());
    return result;
  }
};

// =========================================================================
// Collision System
// =========================================================================

class CollisionSystem {
  std::vector<Collider> colliders;
  SpatialHashGrid broadPhase;
  std::vector<CollisionResult> frameCollisions;

public:
  CollisionSystem() : broadPhase(15.0f, 32) {}

  void Clear() {
    colliders.clear();
    frameCollisions.clear();
  }

  uint32_t AddCollider(const Collider &col) {
    uint32_t idx = static_cast<uint32_t>(colliders.size());
    colliders.push_back(col);
    return idx;
  }

  void UpdatePosition(uint32_t colliderIndex, const Vec3 &worldPos) {
    if (colliderIndex < colliders.size()) {
      colliders[colliderIndex].offset = worldPos;
    }
  }

  // Run collision detection for all colliders
  const std::vector<CollisionResult> &DetectCollisions() {
    frameCollisions.clear();
    broadPhase.Clear();

    // Broad phase: insert into spatial hash
    for (size_t i = 0; i < colliders.size(); i++) {
      float r = GetColliderRadius(colliders[i]);
      broadPhase.Insert(static_cast<uint32_t>(i), colliders[i].offset, r);
    }

    // Narrow phase: test pairs from spatial hash
    for (size_t i = 0; i < colliders.size(); i++) {
      float r = GetColliderRadius(colliders[i]);
      auto candidates = broadPhase.Query(colliders[i].offset, r);

      for (uint32_t j : candidates) {
        if (j <= i)
          continue; // Already tested or self

        // Layer mask check
        if ((colliders[i].layer & colliders[j].layer) == 0)
          continue;

        auto result = NarrowPhaseTest(colliders[i], colliders[j]);
        if (result.hit) {
          result.entityA = colliders[i].entityId;
          result.entityB = colliders[j].entityId;
          frameCollisions.push_back(result);
        }
      }
    }

    return frameCollisions;
  }

  const std::vector<CollisionResult> &GetCollisions() const {
    return frameCollisions;
  }

  // Raycast against all colliders
  bool Raycast(const Vec3 &origin, const Vec3 &direction, float maxDist,
               CollisionResult &outResult) const {
    float closestT = maxDist;
    bool anyHit = false;

    for (const auto &col : colliders) {
      float t = 0;
      if (RaySphereTest(origin, direction, col.offset, GetColliderRadius(col),
                        t)) {
        if (t < closestT && t >= 0) {
          closestT = t;
          outResult.hit = true;
          outResult.contactPoint = origin + direction * t;
          outResult.contactNormal =
              (outResult.contactPoint - col.offset).Normalized();
          outResult.entityA = col.entityId;
          outResult.penetrationDepth = 0;
          anyHit = true;
        }
      }
    }
    return anyHit;
  }

private:
  static float GetColliderRadius(const Collider &c) {
    switch (c.type) {
    case ColliderType::Sphere:
      return c.sphere.radius;
    case ColliderType::Capsule:
      return c.capsule.radius + c.capsule.height * 0.5f;
    case ColliderType::AABB: {
      float mx = std::max(
          {c.box.halfExtents[0], c.box.halfExtents[1], c.box.halfExtents[2]});
      return mx * 1.732f; // sqrt(3)
    }
    }
    return 1.0f;
  }

  static CollisionResult NarrowPhaseTest(const Collider &a, const Collider &b) {
    CollisionResult result;

    // Sphere vs Sphere (most common case in dinosaur sim)
    if (a.type == ColliderType::Sphere && b.type == ColliderType::Sphere) {
      Vec3 delta = b.offset - a.offset;
      float dist = delta.Length();
      float rSum = a.sphere.radius + b.sphere.radius;
      if (dist < rSum && dist > 0.0001f) {
        result.hit = true;
        result.contactNormal = delta * (1.0f / dist);
        result.penetrationDepth = rSum - dist;
        result.contactPoint = a.offset + result.contactNormal * a.sphere.radius;
      }
      return result;
    }

    // Sphere vs AABB
    if ((a.type == ColliderType::Sphere && b.type == ColliderType::AABB) ||
        (a.type == ColliderType::AABB && b.type == ColliderType::Sphere)) {
      const Collider &sphere = (a.type == ColliderType::Sphere) ? a : b;
      const Collider &box = (a.type == ColliderType::AABB) ? a : b;

      Vec3 closest;
      closest.x =
          std::clamp(sphere.offset.x, box.offset.x - box.box.halfExtents[0],
                     box.offset.x + box.box.halfExtents[0]);
      closest.y =
          std::clamp(sphere.offset.y, box.offset.y - box.box.halfExtents[1],
                     box.offset.y + box.box.halfExtents[1]);
      closest.z =
          std::clamp(sphere.offset.z, box.offset.z - box.box.halfExtents[2],
                     box.offset.z + box.box.halfExtents[2]);

      Vec3 delta = sphere.offset - closest;
      float distSq = delta.LengthSq();
      if (distSq < sphere.sphere.radius * sphere.sphere.radius &&
          distSq > 0.0001f) {
        float dist = std::sqrt(distSq);
        result.hit = true;
        result.contactNormal = delta * (1.0f / dist);
        result.penetrationDepth = sphere.sphere.radius - dist;
        result.contactPoint = closest;
      }
    }

    return result;
  }

  static bool RaySphereTest(const Vec3 &origin, const Vec3 &dir,
                            const Vec3 &center, float radius, float &t) {
    Vec3 oc = origin - center;
    float b = oc.Dot(dir);
    float c = oc.Dot(oc) - radius * radius;
    float discriminant = b * b - c;
    if (discriminant < 0)
      return false;
    t = -b - std::sqrt(discriminant);
    return t >= 0;
  }
};

} // namespace Physics
} // namespace Mesozoic
