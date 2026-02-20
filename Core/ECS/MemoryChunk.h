#pragma once
#include <cstdint>
#include <vector>
#include <array>
#include <memory>

namespace Mesozoic {
namespace Core {
namespace ECS {

    // 16KB Chunk Size for Cache Locality (L1/L2 Friendly)
    constexpr size_t CHUNK_SIZE = 16 * 1024;
    
    // Header size is minimal to leave room for data
    struct ChunkHeader {
        uint32_t archetypeId;
        uint16_t count;
        uint16_t capacity;
    };

    struct MemoryChunk {
        ChunkHeader header;
        // Raw byte array for component data.
        // Data is laid out as Structure of Arrays (SoA) within the chunk
        // based on the Archetype's component strides.
        uint8_t data[CHUNK_SIZE - sizeof(ChunkHeader)];

        std::vector<uint32_t> entityIds;

        MemoryChunk(uint32_t archId, uint16_t cap) {
            header.archetypeId = archId;
            header.count = 0;
            header.capacity = cap;
            entityIds.reserve(cap);
        }
    };

} // namespace ECS
} // namespace Core
} // namespace Mesozoic
