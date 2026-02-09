#pragma once
#include "../world/Chunk.h"
#include "../world/ChunkManager.h"
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

constexpr float FACE_SHADE[6] = {
  0.8f,   // +X (East)
  0.8f,   // -X (West) 
  1.0f,   // +Y (Top) - full sunlight
  0.5f,   // -Y (Bottom) - darkest
  0.6f,   // +Z (South)
  0.6f    // -Z (North)
};

void calculateSkyLight(Chunk &c, ChunkManager &chunkManager);

void buildChunkMesh(Chunk &c, ChunkManager &chunkManager);
void uploadToGPU(Chunk &c, const std::vector<Vertex> &verts, const std::vector<uint32_t> &inds);
void uploadWaterToGPU(Chunk &c, const std::vector<Vertex> &verts, const std::vector<uint32_t> &inds);

using BlockGetter = std::function<BlockID(int x, int y, int z)>;
using LightGetter = std::function<uint8_t(int x, int y, int z)>;

void buildChunkMeshOffThread(
    const BlockID* blocks,
    const uint8_t* skyLight,
    BlockGetter getBlock,
    LightGetter getSkyLight,
    std::vector<Vertex>& outVertices,
    std::vector<uint32_t>& outIndices,
    std::vector<Vertex>& outWaterVertices,
    std::vector<uint32_t>& outWaterIndices
);