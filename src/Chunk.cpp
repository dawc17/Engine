#include "Chunk.h"
#include <algorithm>

const glm::ivec3 DIRS[6] = {
    {1, 0, 0},
    {-1, 0, 0},
    {0, 1, 0},
    {0, -1, 0},
    {0, 0, 1},
    {0, 0, -1}
};

Chunk::Chunk()
    : position(0)
{
  std::fill(std::begin(blocks), std::end(blocks), 0);
  std::fill(std::begin(skyLight), std::end(skyLight), MAX_SKY_LIGHT);  // Default to full light
}

Chunk::~Chunk()
{
  if (vao)
    glDeleteVertexArrays(1, &vao);
  if (vbo)
    glDeleteBuffers(1, &vbo);
  if (ebo)
    glDeleteBuffers(1, &ebo);
  if (waterVao)
    glDeleteVertexArrays(1, &waterVao);
  if (waterVbo)
    glDeleteBuffers(1, &waterVbo);
  if (waterEbo)
    glDeleteBuffers(1, &waterEbo);
}
