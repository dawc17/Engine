#pragma once
#include "Chunk.h"
#include <unordered_map>
#include <cstdint>

inline int64_t chunkKey(int x, int y, int z)
{
  return ((int64_t)x << 42) ^ ((int64_t)y << 21) ^ (int64_t)z;
}

struct ChunkManager
{
  std::unordered_map<int64_t, Chunk *> chunks;

  Chunk *getChunk(int cx, int cy, int cz);
  bool hasChunk(int cx, int cy, int cz);
  Chunk *loadChunk(int cx, int cy, int cz);
  void unloadChunk(int cx, int cy, int cz);
};
