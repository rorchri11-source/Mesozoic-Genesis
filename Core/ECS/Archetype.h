#pragma once
#include "MemoryChunk.h"
#include <algorithm>
#include <cstdint>
#include <unordered_map>
#include <vector>


namespace Mesozoic {
namespace Core {
namespace ECS {

using ComponentID = uint32_t;

struct ComponentInfo {
  ComponentID id;
  size_t size;
  size_t alignment;
};

class Archetype {
public:
  uint32_t id;
  std::vector<ComponentInfo> components;
  std::vector<std::unique_ptr<MemoryChunk>> chunks;
  size_t entitySize;
  uint16_t entitiesPerChunk;

  Archetype(uint32_t id, std::vector<ComponentInfo> comps)
      : id(id), components(std::move(comps)) {

    // Calculate layout and capacity
    // For simplicity in this demo, assuming simple packing.
    // Real engine would do optimized packing or separate arrays per component
    // (SoA). Here we'll stick to SoA concept: Chunk Data:
    // [CompA...][CompB...][CompC...]

    size_t totalSize = 0;
    for (const auto &comp : components) {
      totalSize += comp.size;
    }
    entitySize = totalSize;

    // Calculate how many entities fit in a chunk (minus header)
    size_t availableSpace = sizeof(MemoryChunk::data);
    entitiesPerChunk = static_cast<uint16_t>(availableSpace / entitySize);
  }

  // Get offset of a specific component type within the chunk for a given index
  size_t GetComponentOffset(ComponentID compId, uint16_t index) const {
    size_t offset = 0;
    for (const auto &comp : components) {
      if (comp.id == compId) {
        return offset + (comp.size * index);
      }
      offset += comp.size * entitiesPerChunk;
    }
    return static_cast<size_t>(-1); // Not found
  }
};

} // namespace ECS
} // namespace Core
} // namespace Mesozoic
