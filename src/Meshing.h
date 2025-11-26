#pragma once
#include "Chunk.h"
#include <vector>
#include <glm/glm.hpp>

struct Vertex
{
  glm::vec3 pos;
  glm::vec2 uv;   // local UV in block units (0..width/height)
  float tileIndex;
};

void buildChunkMesh(Chunk &c);
void uploadToGPU(Chunk &c, const std::vector<Vertex> &verts, const std::vector<uint32_t> &inds);
