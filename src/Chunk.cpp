#include "Chunk.h"
#include <algorithm>

const glm::ivec3 DIRS[6] = {
    {1, 0, 0},  // +X
    {-1, 0, 0}, // -X
    {0, 1, 0},  // +Y
    {0, -1, 0}, // -Y
    {0, 0, 1},  // +Z
    {0, 0, -1}  // -Z
};

Chunk::Chunk()
    : position(0)
{
  std::fill(std::begin(blocks), std::end(blocks), 0);
}

Chunk::~Chunk()
{
  if (vao)
    glDeleteVertexArrays(1, &vao);
  if (vbo)
    glDeleteBuffers(1, &vbo);
  if (ebo)
    glDeleteBuffers(1, &ebo);
}
