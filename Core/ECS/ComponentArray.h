#pragma once
#include <cassert>
#include <cstdint>
#include <unordered_map>
#include <vector>


namespace Mesozoic {
namespace Core {
namespace ECS {

// Base class for generic component handling
class IComponentArray {
public:
  virtual ~IComponentArray() = default;
  virtual void EntityDestroyed(uint32_t entityId) = 0;
  virtual size_t Size() const = 0;
};

// Sparse-Set implementation for random-access component storage.
// Used alongside chunk-based iteration for tools, debug, and sparse lookups.
// Main simulation should iterate chunks directly for cache locality.
template <typename T> class ComponentArray : public IComponentArray {
public:
  void InsertData(uint32_t entityId, T component) {
    assert(entityToIndex.find(entityId) == entityToIndex.end() &&
           "Component already exists for entity");

    size_t newIndex = packedData.size();
    entityToIndex[entityId] = newIndex;
    indexToEntity[newIndex] = entityId;
    packedData.push_back(std::move(component));
  }

  void RemoveData(uint32_t entityId) {
    auto it = entityToIndex.find(entityId);
    if (it == entityToIndex.end())
      return;

    size_t removedIndex = it->second;
    size_t lastIndex = packedData.size() - 1;

    if (removedIndex != lastIndex) {
      // Move last element into removed slot
      packedData[removedIndex] = std::move(packedData[lastIndex]);

      // Update maps for the moved element
      uint32_t lastEntity = indexToEntity[lastIndex];
      entityToIndex[lastEntity] = removedIndex;
      indexToEntity[removedIndex] = lastEntity;
    }

    packedData.pop_back();
    entityToIndex.erase(entityId);
    indexToEntity.erase(lastIndex);
  }

  T &GetData(uint32_t entityId) {
    auto it = entityToIndex.find(entityId);
    assert(it != entityToIndex.end() && "Entity does not have this component");
    return packedData[it->second];
  }

  const T &GetData(uint32_t entityId) const {
    auto it = entityToIndex.find(entityId);
    assert(it != entityToIndex.end() && "Entity does not have this component");
    return packedData[it->second];
  }

  bool HasData(uint32_t entityId) const {
    return entityToIndex.find(entityId) != entityToIndex.end();
  }

  void EntityDestroyed(uint32_t entityId) override {
    if (entityToIndex.find(entityId) != entityToIndex.end()) {
      RemoveData(entityId);
    }
  }

  size_t Size() const override { return packedData.size(); }

  // Direct access to packed array for iteration
  T *Data() { return packedData.data(); }
  const T *Data() const { return packedData.data(); }

  // Get entity ID at packed index
  uint32_t GetEntityAtIndex(size_t index) const {
    auto it = indexToEntity.find(index);
    return (it != indexToEntity.end()) ? it->second : UINT32_MAX;
  }

private:
  std::vector<T> packedData;
  std::unordered_map<uint32_t, size_t> entityToIndex;
  std::unordered_map<size_t, uint32_t> indexToEntity;
};

} // namespace ECS
} // namespace Core
} // namespace Mesozoic
