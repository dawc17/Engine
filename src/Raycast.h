#pragma once
#include <glm/glm.hpp>
#include <optional>

struct ChunkManager;

struct RaycastHit
{
  glm::ivec3 blockPos;   // World block position
  glm::ivec3 normal;     // Face normal (direction to place new block)
  float distance;        // Distance from ray origin
};

// DDA voxel raycast algorithm
// Returns the first solid block hit within maxDistance
std::optional<RaycastHit> raycastVoxel(
    const glm::vec3& origin,
    const glm::vec3& direction,
    float maxDistance,
    ChunkManager& chunkManager);

// Get block at world position
uint8_t getBlockAtWorld(int wx, int wy, int wz, ChunkManager& chunkManager);

// Set block at world position and mark affected chunks dirty
void setBlockAtWorld(int wx, int wy, int wz, uint8_t blockId, ChunkManager& chunkManager);
