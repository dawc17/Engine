#include "Raycast.h"
#include "../world/ChunkManager.h"
#include "../world/Chunk.h"
#include "../utils/BlockTypes.h"
#include "../utils/CoordUtils.h"
#include <cmath>

uint8_t getBlockAtWorld(int wx, int wy, int wz, ChunkManager& chunkManager)
{
  glm::ivec3 chunk = worldToChunk(wx, wy, wz);
  Chunk* c = chunkManager.getChunk(chunk.x, chunk.y, chunk.z);
  if (!c)
    return 0;

  glm::ivec3 local = worldToLocal(wx, wy, wz);
  return c->blocks[blockIndex(local.x, local.y, local.z)];
}

void setBlockAtWorld(int wx, int wy, int wz, uint8_t blockId, ChunkManager& chunkManager)
{
  glm::ivec3 chunk = worldToChunk(wx, wy, wz);
  Chunk* c = chunkManager.getChunk(chunk.x, chunk.y, chunk.z);
  if (!c)
    return;

  glm::ivec3 local = worldToLocal(wx, wy, wz);
  c->blocks[blockIndex(local.x, local.y, local.z)] = blockId;
  c->dirtyMesh = true;
  c->dirtyLight = true;

  for (int i = 0; i < 6; i++)
  {
    glm::ivec3 neighborLocal = local + DIRS[i];
    bool onBoundary = neighborLocal.x < 0 || neighborLocal.x >= CHUNK_SIZE ||
                      neighborLocal.y < 0 || neighborLocal.y >= CHUNK_SIZE ||
                      neighborLocal.z < 0 || neighborLocal.z >= CHUNK_SIZE;
    if (onBoundary)
    {
      glm::ivec3 neighborChunk = chunk + DIRS[i];
      Chunk* neighbor = chunkManager.getChunk(neighborChunk.x, neighborChunk.y, neighborChunk.z);
      if (neighbor)
      {
        neighbor->dirtyMesh = true;
        neighbor->dirtyLight = true;
      }
    }
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
    if (isBlockSolid(block))
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
