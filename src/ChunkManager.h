#pragma once
#include "Chunk.h"
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <unordered_map>

struct ChunkManager
{
  struct ChunkCoord
  {
    int x;
    int y;
    int z;

    bool operator==(const ChunkCoord &other) const noexcept
    {
      return x == other.x && y == other.y && z == other.z;
    }
  };

  struct ChunkCoordHash
  {
    std::size_t operator()(const ChunkCoord &coord) const noexcept
    {
      std::size_t h = std::hash<int>{}(coord.x);
      h ^= std::hash<int>{}(coord.y) + 0x9e3779b9 + (h << 6) + (h >> 2);
      h ^= std::hash<int>{}(coord.z) + 0x9e3779b9 + (h << 6) + (h >> 2);
      return h;
    }
  };

  using ChunkMap = std::unordered_map<ChunkCoord, std::unique_ptr<Chunk>, ChunkCoordHash>;

  ChunkMap chunks;

  Chunk *getChunk(int cx, int cy, int cz);
  bool hasChunk(int cx, int cy, int cz);
  Chunk *loadChunk(int cx, int cy, int cz);
  void unloadChunk(int cx, int cy, int cz);
};
