#pragma once
#include <cstddef>
#include <functional>
#include <cmath>
#include "../world/Chunk.h"

struct IVec3Hash
{
    std::size_t operator()(const glm::ivec3& v) const noexcept
    {
        std::size_t h = std::hash<int>{}(v.x);
        h ^= std::hash<int>{}(v.y) + 0x9e3779b9 + (h << 6) + (h >> 2);
        h ^= std::hash<int>{}(v.z) + 0x9e3779b9 + (h << 6) + (h >> 2);
        return h;
    }
};

struct IVec2Hash
{
    std::size_t operator()(const glm::ivec2& v) const noexcept
    {
        std::size_t h = std::hash<int>{}(v.x);
        h ^= std::hash<int>{}(v.y) + 0x9e3779b9 + (h << 6) + (h >> 2);
        return h;
    }
};

inline glm::ivec3 worldToChunk(int wx, int wy, int wz)
{
    return glm::ivec3(
        static_cast<int>(std::floor(static_cast<float>(wx) / CHUNK_SIZE)),
        static_cast<int>(std::floor(static_cast<float>(wy) / CHUNK_SIZE)),
        static_cast<int>(std::floor(static_cast<float>(wz) / CHUNK_SIZE))
    );
}

inline glm::ivec3 worldToLocal(int wx, int wy, int wz)
{
    return glm::ivec3(
        ((wx % CHUNK_SIZE) + CHUNK_SIZE) % CHUNK_SIZE,
        ((wy % CHUNK_SIZE) + CHUNK_SIZE) % CHUNK_SIZE,
        ((wz % CHUNK_SIZE) + CHUNK_SIZE) % CHUNK_SIZE
    );
}

inline glm::ivec3 chunkToWorld(int cx, int cy, int cz)
{
    return glm::ivec3(cx * CHUNK_SIZE, cy * CHUNK_SIZE, cz * CHUNK_SIZE);
}
