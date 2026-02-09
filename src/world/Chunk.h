#pragma once
#include <cstdint>
#include <glm/glm.hpp>
#include <glad/glad.h>

using BlockID = uint8_t;
constexpr int CHUNK_SIZE = 16;
constexpr int CHUNK_VOLUME = CHUNK_SIZE * CHUNK_SIZE * CHUNK_SIZE;

// Skylight levels (Minecraft uses 0-15, we use 0-15 too)
constexpr uint8_t MAX_SKY_LIGHT = 15;

struct Chunk
{
  Chunk();
  ~Chunk();

  glm::ivec3 position;
  BlockID blocks[CHUNK_VOLUME];
  uint8_t skyLight[CHUNK_VOLUME];

  bool dirtyMesh = true;
  bool dirtyLight = true;
  GLuint vao = 0, vbo = 0, ebo = 0;
  uint32_t indexCount = 0;
  uint32_t vertexCount = 0;

  GLuint waterVao = 0, waterVbo = 0, waterEbo = 0;
  uint32_t waterIndexCount = 0;
  uint32_t waterVertexCount = 0;
};

extern const glm::ivec3 DIRS[6];

inline int blockIndex(int x, int y, int z)
{
  return x + CHUNK_SIZE * (y + CHUNK_SIZE * z);
}
