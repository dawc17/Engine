#include "Player.h"
#include "ChunkManager.h"
#include "Raycast.h"
#include <cmath>
#include <algorithm>

// AABB implementation
bool AABB::intersects(const AABB& other) const
{
  return (min.x < other.max.x && max.x > other.min.x) &&
         (min.y < other.max.y && max.y > other.min.y) &&
         (min.z < other.max.z && max.z > other.min.z);
}

AABB AABB::fromBlockPos(int x, int y, int z)
{
  AABB aabb;
  aabb.min = glm::vec3(static_cast<float>(x), static_cast<float>(y), static_cast<float>(z));
  aabb.max = aabb.min + glm::vec3(1.0f, 1.0f, 1.0f);
  return aabb;
}

// Player implementation
Player::Player()
    : position(0.0f, 70.0f, 0.0f)  // Start high above terrain, will fall to ground
    , velocity(0.0f)
    , yaw(-90.0f)
    , pitch(0.0f)
    , onGround(false)
    , noclip(false)
{
}

AABB Player::getAABB() const
{
  AABB aabb;
  float halfWidth = PLAYER_WIDTH * 0.5f;
  float halfDepth = PLAYER_DEPTH * 0.5f;
  
  aabb.min = glm::vec3(
      position.x - halfWidth,
      position.y,
      position.z - halfDepth);
  aabb.max = glm::vec3(
      position.x + halfWidth,
      position.y + PLAYER_HEIGHT,
      position.z + halfDepth);
  
  return aabb;
}

glm::vec3 Player::getEyePosition() const
{
  return position + glm::vec3(0.0f, PLAYER_EYE_HEIGHT, 0.0f);
}

void Player::applyMovement(const glm::vec3& inputDir, float speed)
{
  if (glm::length(inputDir) > 0.0f)
  {
    glm::vec3 normalizedInput = glm::normalize(inputDir);
    velocity.x = normalizedInput.x * speed;
    velocity.z = normalizedInput.z * speed;
    
    // Handle Y velocity for noclip mode
    if (noclip)
    {
      velocity.y = normalizedInput.y * speed;
    }
  }
  else
  {
    velocity.x = 0.0f;
    velocity.z = 0.0f;
    
    if (noclip)
    {
      velocity.y = 0.0f;
    }
  }
}

void Player::jump()
{
  if (onGround)
  {
    velocity.y = JUMP_VELOCITY;
    onGround = false;
  }
}

// Check if a block at world position is solid
static bool isBlockSolid(int wx, int wy, int wz, ChunkManager& chunkManager)
{
  uint8_t blockId = getBlockAtWorld(wx, wy, wz, chunkManager);
  return blockId != 0;  // 0 = air
}

// Get all block AABBs that might collide with the player AABB
static std::vector<AABB> getCollidingBlocks(const AABB& playerAABB, ChunkManager& chunkManager)
{
  std::vector<AABB> blocks;
  
  // Expand search area by 1 block in each direction
  int minX = static_cast<int>(std::floor(playerAABB.min.x));
  int minY = static_cast<int>(std::floor(playerAABB.min.y));
  int minZ = static_cast<int>(std::floor(playerAABB.min.z));
  int maxX = static_cast<int>(std::floor(playerAABB.max.x));
  int maxY = static_cast<int>(std::floor(playerAABB.max.y));
  int maxZ = static_cast<int>(std::floor(playerAABB.max.z));
  
  for (int x = minX; x <= maxX; x++)
  {
    for (int y = minY; y <= maxY; y++)
    {
      for (int z = minZ; z <= maxZ; z++)
      {
        if (isBlockSolid(x, y, z, chunkManager))
        {
          blocks.push_back(AABB::fromBlockPos(x, y, z));
        }
      }
    }
  }
  
  return blocks;
}

// Swept AABB collision: move player along one axis and resolve collisions
// Returns the actual movement that occurred
static float moveAxis(
    glm::vec3& position,
    float movement,
    int axis,  // 0=X, 1=Y, 2=Z
    ChunkManager& chunkManager,
    bool& hitSomething)
{
  hitSomething = false;
  
  if (std::abs(movement) < 0.0001f)
    return 0.0f;
  
  float halfWidth = PLAYER_WIDTH * 0.5f;
  float halfDepth = PLAYER_DEPTH * 0.5f;
  
  // Create AABB at new position
  glm::vec3 newPos = position;
  newPos[axis] += movement;
  
  AABB newAABB;
  newAABB.min = glm::vec3(
      newPos.x - halfWidth,
      newPos.y,
      newPos.z - halfDepth);
  newAABB.max = glm::vec3(
      newPos.x + halfWidth,
      newPos.y + PLAYER_HEIGHT,
      newPos.z + halfDepth);
  
  // Check for collisions
  std::vector<AABB> blocks = getCollidingBlocks(newAABB, chunkManager);
  
  for (const AABB& block : blocks)
  {
    if (newAABB.intersects(block))
    {
      hitSomething = true;
      
      // Resolve collision by pushing player out
      if (movement > 0)
      {
        // Moving in positive direction, push back to block's min face
        if (axis == 0)
          newPos.x = block.min.x - halfWidth - 0.0001f;
        else if (axis == 1)
          newPos.y = block.min.y - PLAYER_HEIGHT - 0.0001f;
        else
          newPos.z = block.min.z - halfDepth - 0.0001f;
      }
      else
      {
        // Moving in negative direction, push forward to block's max face
        if (axis == 0)
          newPos.x = block.max.x + halfWidth + 0.0001f;
        else if (axis == 1)
          newPos.y = block.max.y + 0.0001f;
        else
          newPos.z = block.max.z + halfDepth + 0.0001f;
      }
      
      // Recalculate AABB after adjustment
      newAABB.min = glm::vec3(
          newPos.x - halfWidth,
          newPos.y,
          newPos.z - halfDepth);
      newAABB.max = glm::vec3(
          newPos.x + halfWidth,
          newPos.y + PLAYER_HEIGHT,
          newPos.z + halfDepth);
    }
  }
  
  float actualMovement = newPos[axis] - position[axis];
  position = newPos;
  return actualMovement;
}

void Player::update(float dt, ChunkManager& chunkManager)
{
  if (noclip)
  {
    // Noclip mode: just move without collision
    position += velocity * dt;
    return;
  }
  
  // Apply gravity
  velocity.y -= GRAVITY * dt;
  
  // Clamp to terminal velocity
  if (velocity.y < -TERMINAL_VELOCITY)
    velocity.y = -TERMINAL_VELOCITY;
  
  // Calculate movement this frame
  glm::vec3 movement = velocity * dt;
  
  // Move along each axis separately (Y first for ground detection)
  bool hitY = false;
  bool hitX = false;
  bool hitZ = false;
  
  // Move Y axis first
  moveAxis(position, movement.y, 1, chunkManager, hitY);
  if (hitY)
  {
    if (velocity.y < 0)
    {
      onGround = true;
    }
    velocity.y = 0.0f;
  }
  else
  {
    onGround = false;
  }
  
  // Move X axis
  moveAxis(position, movement.x, 0, chunkManager, hitX);
  if (hitX)
  {
    velocity.x = 0.0f;
  }
  
  // Move Z axis
  moveAxis(position, movement.z, 2, chunkManager, hitZ);
  if (hitZ)
  {
    velocity.z = 0.0f;
  }
}

glm::vec3 playerForwardXZ(const Player& player)
{
  glm::vec3 forward;
  forward.x = cos(glm::radians(player.yaw));
  forward.y = 0.0f;
  forward.z = sin(glm::radians(player.yaw));
  return glm::normalize(forward);
}

glm::vec3 playerRightXZ(const Player& player)
{
  glm::vec3 forward = playerForwardXZ(player);
  return glm::normalize(glm::cross(forward, glm::vec3(0.0f, 1.0f, 0.0f)));
}

