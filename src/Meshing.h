#pragma once
#include "Chunk.h"
#include "ChunkManager.h"
#include <vector>
#include <functional>
#include <glm/glm.hpp>

struct Vertex
{
  glm::vec3 pos;
  glm::vec2 uv;
  float tileIndex;
  float skyLight;
  float faceShade;
};

// Face direction indices for lighting (matches DIRS array order)
enum FaceDir {
  DIR_POS_X = 0,
  DIR_NEG_X = 1,
  DIR_POS_Y = 2,
  DIR_NEG_Y = 3,
  DIR_POS_Z = 4,
  DIR_NEG_Z = 5
};

// Base light levels for each face direction (Minecraft-style directional lighting)
// Top faces get full light, sides get varying amounts, bottom gets least
constexpr float FACE_SHADE[6] = {
  0.8f,   // +X (East)
  0.8f,   // -X (West) 
  1.0f,   // +Y (Top) - full sunlight
  0.5f,   // -Y (Bottom) - darkest
  0.6f,   // +Z (South)
  0.6f    // -Z (North)
};

// Calculate skylight for a chunk (propagates from sky downward)
void calculateSkyLight(Chunk &c, ChunkManager &chunkManager);

void buildChunkMesh(Chunk &c, ChunkManager &chunkManager);
void uploadToGPU(Chunk &c, const std::vector<Vertex> &verts, const std::vector<uint32_t> &inds);

using BlockGetter = std::function<BlockID(int x, int y, int z)>;
void buildChunkMeshOffThread(
    const BlockID* blocks,
    BlockGetter getBlock,
    std::vector<Vertex>& outVertices,
    std::vector<uint32_t>& outIndices
);