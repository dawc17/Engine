#include "ChunkManager.h"
#include <iostream>

bool ChunkManager::hasChunk(int cx, int cy, int cz)
{
  ChunkCoord key{cx, cy, cz};
  return chunks.find(key) != chunks.end();
}

namespace
{
  void generateFlatTerrain(Chunk &c)
  {
    for (int x = 0; x < CHUNK_SIZE; x++)
    {
      for (int z = 0; z < CHUNK_SIZE; z++)
      {
        for (int y = 0; y < CHUNK_SIZE; y++)
        {
          int i = blockIndex(x, y, z);
          if (y < 5)
            c.blocks[i] = 3; // stone
          else if (y < 9)
            c.blocks[i] = 1; // dirt
          else if (y < 10)
            c.blocks[i] = 2; // grass (top layer only)
          else
            c.blocks[i] = 0; // air
        }
      }
    }
  }
}

Chunk *ChunkManager::getChunk(int cx, int cy, int cz)
{
  ChunkCoord key{cx, cy, cz};
  auto it = chunks.find(key);
  if (it == chunks.end())
    return nullptr;
  return it->second.get();
}

Chunk *ChunkManager::loadChunk(int cx, int cy, int cz)
{
  if (hasChunk(cx, cy, cz))
    return getChunk(cx, cy, cz);

  std::cout << "Loading chunk at (" << cx << ", " << cy << ", " << cz << ")" << std::endl;

  ChunkCoord key{cx, cy, cz};
  auto [it, inserted] = chunks.emplace(key, std::make_unique<Chunk>());
  (void)inserted;
  Chunk *c = it->second.get();
  c->position = {cx, cy, cz};

  glGenVertexArrays(1, &c->vao);
  glGenBuffers(1, &c->vbo);
  glGenBuffers(1, &c->ebo);

  generateFlatTerrain(*c);

  return c;
}

void ChunkManager::unloadChunk(int cx, int cy, int cz)
{
  ChunkCoord key{cx, cy, cz};
  auto it = chunks.find(key);

  if (it != chunks.end())
  {
    chunks.erase(it);
  }
}
