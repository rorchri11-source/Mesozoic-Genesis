#pragma once
#include "Archetype.h"
#include <cstdint>
#include <cstring>
#include <functional>
#include <queue>
#include <unordered_map>
#include <vector>

namespace Mesozoic {
namespace Core {
namespace ECS {

using EntityID = uint32_t;
static constexpr EntityID INVALID_ENTITY = static_cast<EntityID>(-1);

struct EntityLocation {
  uint32_t archetypeId;
  uint32_t chunkIndex;
  uint16_t indexInChunk;
  bool valid = false;
};

class EntityManager {
public:
  static constexpr uint32_t MAX_ENTITIES = 100000;

  EntityManager() {
    entityLocations.resize(MAX_ENTITIES);
    for (EntityID i = 0; i < MAX_ENTITIES; ++i) {
      availableEntities.push(i);
    }
  }

  // Register an archetype for a component signature
  uint32_t RegisterArchetype(std::vector<ComponentInfo> components) {
    uint64_t signature = ComputeSignature(components);
    auto it = signatureToArchetype.find(signature);
    if (it != signatureToArchetype.end()) {
      return it->second;
    }

    uint32_t archId = static_cast<uint32_t>(archetypes.size());
    archetypes.push_back(
        std::make_unique<Archetype>(archId, std::move(components)));
    signatureToArchetype[signature] = archId;

    // Allocate first chunk
    AllocateChunk(archId);
    return archId;
  }

  // Create an entity in the given archetype
  EntityID CreateEntity(uint32_t archetypeId) {
    if (availableEntities.empty())
      return INVALID_ENTITY;
    if (archetypeId >= archetypes.size())
      return INVALID_ENTITY;

    EntityID id = availableEntities.front();
    availableEntities.pop();
    livingEntityCount++;

    Archetype *arch = archetypes[archetypeId].get();

    // Find a chunk with space, or allocate a new one
    uint32_t chunkIdx = UINT32_MAX;
    for (uint32_t i = 0; i < static_cast<uint32_t>(arch->chunks.size()); ++i) {
      if (arch->chunks[i]->header.count < arch->chunks[i]->header.capacity) {
        chunkIdx = i;
        break;
      }
    }
    if (chunkIdx == UINT32_MAX) {
      chunkIdx = AllocateChunk(archetypeId);
    }

    MemoryChunk *chunk = arch->chunks[chunkIdx].get();
    uint16_t indexInChunk = chunk->header.count;
    chunk->header.count++;
    chunk->entityIds.push_back(id);

    // Record location
    entityLocations[id].archetypeId = archetypeId;
    entityLocations[id].chunkIndex = chunkIdx;
    entityLocations[id].indexInChunk = indexInChunk;
    entityLocations[id].valid = true;

    return id;
  }

  void DestroyEntity(EntityID entity) {
    if (entity >= MAX_ENTITIES || !entityLocations[entity].valid)
      return;

    auto &loc = entityLocations[entity];
    Archetype *arch = archetypes[loc.archetypeId].get();
    MemoryChunk *chunk = arch->chunks[loc.chunkIndex].get();

    // Swap-remove: move last entity into deleted slot
    uint16_t lastIndex = chunk->header.count - 1;
    if (loc.indexInChunk != lastIndex) {
      // Copy component data from last slot to deleted slot
      for (const auto &comp : arch->components) {
        size_t srcOffset = arch->GetComponentOffset(comp.id, lastIndex);
        size_t dstOffset = arch->GetComponentOffset(comp.id, loc.indexInChunk);
        if (srcOffset != static_cast<size_t>(-1) &&
            dstOffset != static_cast<size_t>(-1)) {
          std::memcpy(&chunk->data[dstOffset], &chunk->data[srcOffset],
                      comp.size);
        }
      }

      // Find the entity that was in lastIndex and update its location
      EntityID movedEntity = chunk->entityIds[lastIndex];
      entityLocations[movedEntity].indexInChunk = loc.indexInChunk;
      chunk->entityIds[loc.indexInChunk] = movedEntity;
    }

    chunk->header.count--;
    chunk->entityIds.pop_back();
    loc.valid = false;
    availableEntities.push(entity);
    livingEntityCount--;
  }

  // Get raw pointer to component data for an entity
  void *GetComponentData(EntityID entity, ComponentID compId) {
    if (entity >= MAX_ENTITIES || !entityLocations[entity].valid)
      return nullptr;

    auto &loc = entityLocations[entity];
    Archetype *arch = archetypes[loc.archetypeId].get();
    MemoryChunk *chunk = arch->chunks[loc.chunkIndex].get();

    size_t offset = arch->GetComponentOffset(compId, loc.indexInChunk);
    if (offset == static_cast<size_t>(-1))
      return nullptr;
    return &chunk->data[offset];
  }

  // Typed accessor
  template <typename T> T *GetComponent(EntityID entity, ComponentID compId) {
    return static_cast<T *>(GetComponentData(entity, compId));
  }

  // Iterate all entities in an archetype (for systems)
  void ForEachInArchetype(
      uint32_t archetypeId,
      const std::function<void(MemoryChunk *, uint16_t)> &callback) {
    if (archetypeId >= archetypes.size())
      return;
    Archetype *arch = archetypes[archetypeId].get();
    for (auto &chunk : arch->chunks) {
      for (uint16_t i = 0; i < chunk->header.count; ++i) {
        callback(chunk.get(), i);
      }
    }
  }

  const EntityLocation &GetLocation(EntityID entity) const {
    return entityLocations[entity];
  }

  uint32_t GetLivingCount() const { return livingEntityCount; }

  Archetype *GetArchetype(uint32_t id) {
    return (id < archetypes.size()) ? archetypes[id].get() : nullptr;
  }

private:
  uint32_t AllocateChunk(uint32_t archetypeId) {
    Archetype *arch = archetypes[archetypeId].get();
    auto chunk =
        std::make_unique<MemoryChunk>(archetypeId, arch->entitiesPerChunk);
    uint32_t idx = static_cast<uint32_t>(arch->chunks.size());
    arch->chunks.push_back(std::move(chunk));
    return idx;
  }

  uint64_t
  ComputeSignature(const std::vector<ComponentInfo> &components) const {
    uint64_t sig = 0;
    for (const auto &c : components) {
      sig ^= static_cast<uint64_t>(c.id) * 2654435761ULL; // Knuth hash
    }
    return sig;
  }

  std::queue<EntityID> availableEntities;
  uint32_t livingEntityCount = 0;
  std::vector<std::unique_ptr<Archetype>> archetypes;
  std::unordered_map<uint64_t, uint32_t> signatureToArchetype;
  std::vector<EntityLocation> entityLocations;
};

} // namespace ECS
} // namespace Core
} // namespace Mesozoic
