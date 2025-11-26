#include "ChunkManager.h"
#include <iostream>

bool ChunkManager::hasChunk(int cx, int cy, int cz)
{
  return chunks.find(chunkKey(cx, cy, cz)) != chunks.end();
}

Chunk *ChunkManager::getChunk(int cx, int cy, int cz)
{
  auto it = chunks.find(chunkKey(cx, cy, cz));
  if (it == chunks.end())
    return nullptr;
  return it->second;
}

Chunk *ChunkManager::loadChunk(int cx, int cy, int cz)
{
  if (hasChunk(cx, cy, cz))
    return getChunk(cx, cy, cz);

  std::cout << "Loading chunk at (" << cx << ", " << cy << ", " << cz << ")" << std::endl;

  Chunk *c = new Chunk();
  c->position = {cx, cy, cz};

  glGenVertexArrays(1, &c->vao);
  glGenBuffers(1, &c->vbo);
  glGenBuffers(1, &c->ebo);

  // fill with terrain - all stone for testing
  for (int x = 0; x < CHUNK_SIZE; x++) {
    for (int z = 0; z < CHUNK_SIZE; z++) {
      for (int y = 0; y < CHUNK_SIZE; y++) {
        int i = blockIndex(x, y, z);
        if (y < 8)
          c->blocks[i] = 2; // stone
        else
          c->blocks[i] = 0; // air
      }
    }
  }

  chunks[chunkKey(cx, cy, cz)] = c;
  return c;
}

void ChunkManager::unloadChunk(int cx, int cy, int cz)
{
  auto key = chunkKey(cx, cy, cz);
  auto it = chunks.find(key);

  if (it != chunks.end())
  {
    Chunk *c = it->second;
    // Clean up OpenGL resources
    glDeleteVertexArrays(1, &c->vao);
    glDeleteBuffers(1, &c->vbo);
    glDeleteBuffers(1, &c->ebo);
    delete c;
    chunks.erase(it);
  }
}
