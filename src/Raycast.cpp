#include "Raycast.h"
#include "ChunkManager.h"
#include "Chunk.h"
#include "BlockTypes.h"
#include <cmath>

uint8_t getBlockAtWorld(int wx, int wy, int wz, ChunkManager& chunkManager)
{
  // Convert world coords to chunk coords
  int cx = static_cast<int>(std::floor(static_cast<float>(wx) / CHUNK_SIZE));
  int cy = static_cast<int>(std::floor(static_cast<float>(wy) / CHUNK_SIZE));
  int cz = static_cast<int>(std::floor(static_cast<float>(wz) / CHUNK_SIZE));

  Chunk* chunk = chunkManager.getChunk(cx, cy, cz);
  if (!chunk)
    return 0; // Air if chunk not loaded

  // Local coords within chunk (handle negative modulo correctly)
  int lx = ((wx % CHUNK_SIZE) + CHUNK_SIZE) % CHUNK_SIZE;
  int ly = ((wy % CHUNK_SIZE) + CHUNK_SIZE) % CHUNK_SIZE;
  int lz = ((wz % CHUNK_SIZE) + CHUNK_SIZE) % CHUNK_SIZE;

  return chunk->blocks[blockIndex(lx, ly, lz)];
}

void setBlockAtWorld(int wx, int wy, int wz, uint8_t blockId, ChunkManager& chunkManager)
{
  // Convert world coords to chunk coords
  int cx = static_cast<int>(std::floor(static_cast<float>(wx) / CHUNK_SIZE));
  int cy = static_cast<int>(std::floor(static_cast<float>(wy) / CHUNK_SIZE));
  int cz = static_cast<int>(std::floor(static_cast<float>(wz) / CHUNK_SIZE));

  Chunk* chunk = chunkManager.getChunk(cx, cy, cz);
  if (!chunk)
    return;

  // Local coords within chunk
  int lx = ((wx % CHUNK_SIZE) + CHUNK_SIZE) % CHUNK_SIZE;
  int ly = ((wy % CHUNK_SIZE) + CHUNK_SIZE) % CHUNK_SIZE;
  int lz = ((wz % CHUNK_SIZE) + CHUNK_SIZE) % CHUNK_SIZE;

  chunk->blocks[blockIndex(lx, ly, lz)] = blockId;
  chunk->dirtyMesh = true;
  chunk->dirtyLight = true;  // Recalculate lighting when blocks change

  // Mark neighboring chunks dirty if block is on a boundary
  if (lx == 0)
  {
    Chunk* neighbor = chunkManager.getChunk(cx - 1, cy, cz);
    if (neighbor) { neighbor->dirtyMesh = true; neighbor->dirtyLight = true; }
  }
  if (lx == CHUNK_SIZE - 1)
  {
    Chunk* neighbor = chunkManager.getChunk(cx + 1, cy, cz);
    if (neighbor) { neighbor->dirtyMesh = true; neighbor->dirtyLight = true; }
  }
  if (ly == 0)
  {
    Chunk* neighbor = chunkManager.getChunk(cx, cy - 1, cz);
    if (neighbor) { neighbor->dirtyMesh = true; neighbor->dirtyLight = true; }
  }
  if (ly == CHUNK_SIZE - 1)
  {
    Chunk* neighbor = chunkManager.getChunk(cx, cy + 1, cz);
    if (neighbor) { neighbor->dirtyMesh = true; neighbor->dirtyLight = true; }
  }
  if (lz == 0)
  {
    Chunk* neighbor = chunkManager.getChunk(cx, cy, cz - 1);
    if (neighbor) { neighbor->dirtyMesh = true; neighbor->dirtyLight = true; }
  }
  if (lz == CHUNK_SIZE - 1)
  {
    Chunk* neighbor = chunkManager.getChunk(cx, cy, cz + 1);
    if (neighbor) { neighbor->dirtyMesh = true; neighbor->dirtyLight = true; }
  }
}

std::optional<RaycastHit> raycastVoxel(
    const glm::vec3& origin,
    const glm::vec3& direction,
    float maxDistance,
    ChunkManager& chunkManager)
{
  // DDA (Digital Differential Analyzer) voxel traversal
  // Based on "A Fast Voxel Traversal Algorithm for Ray Tracing" by Amanatides & Woo

  glm::vec3 dir = glm::normalize(direction);

  // Current voxel position (integer)
  glm::ivec3 voxel(
      static_cast<int>(std::floor(origin.x)),
      static_cast<int>(std::floor(origin.y)),
      static_cast<int>(std::floor(origin.z)));

  // Direction to step in each axis (+1 or -1)
  glm::ivec3 step(
      (dir.x >= 0) ? 1 : -1,
      (dir.y >= 0) ? 1 : -1,
      (dir.z >= 0) ? 1 : -1);

  // How far along the ray we must move for each component to cross a voxel boundary
  // (in units of t)
  glm::vec3 tDelta(
      (dir.x != 0.0f) ? std::abs(1.0f / dir.x) : 1e30f,
      (dir.y != 0.0f) ? std::abs(1.0f / dir.y) : 1e30f,
      (dir.z != 0.0f) ? std::abs(1.0f / dir.z) : 1e30f);

  // Distance to the next voxel boundary for each axis
  glm::vec3 tMax;
  if (dir.x >= 0)
    tMax.x = ((voxel.x + 1) - origin.x) / dir.x;
  else
    tMax.x = (origin.x - voxel.x) / (-dir.x);

  if (dir.y >= 0)
    tMax.y = ((voxel.y + 1) - origin.y) / dir.y;
  else
    tMax.y = (origin.y - voxel.y) / (-dir.y);

  if (dir.z >= 0)
    tMax.z = ((voxel.z + 1) - origin.z) / dir.z;
  else
    tMax.z = (origin.z - voxel.z) / (-dir.z);

  float distance = 0.0f;
  glm::ivec3 lastNormal(0);

  while (distance < maxDistance)
  {
    uint8_t block = getBlockAtWorld(voxel.x, voxel.y, voxel.z, chunkManager);
    if (block != 0 && g_blockTypes[block].solid)
    {
      RaycastHit hit;
      hit.blockPos = voxel;
      hit.normal = lastNormal;
      hit.distance = distance;
      return hit;
    }

    // Step to next voxel
    if (tMax.x < tMax.y && tMax.x < tMax.z)
    {
      voxel.x += step.x;
      distance = tMax.x;
      tMax.x += tDelta.x;
      lastNormal = glm::ivec3(-step.x, 0, 0);
    }
    else if (tMax.y < tMax.z)
    {
      voxel.y += step.y;
      distance = tMax.y;
      tMax.y += tDelta.y;
      lastNormal = glm::ivec3(0, -step.y, 0);
    }
    else
    {
      voxel.z += step.z;
      distance = tMax.z;
      tMax.z += tDelta.z;
      lastNormal = glm::ivec3(0, 0, -step.z);
    }
  }

  return std::nullopt;
}
