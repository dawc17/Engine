#include "Player.h"
#include "../world/ChunkManager.h"
#include "Raycast.h"
#include "../utils/BlockTypes.h"
#include <cmath>

static thread_local std::vector<AABB> s_collisionScratch;

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

Player::Player()
    : position(0.0f, 70.0f, 0.0f)
    , velocity(0.0f)
    , yaw(-90.0f)
    , pitch(0.0f)
    , onGround(false)
    , noclip(false)
    , gamemode(Gamemode::Creative)
    , health(20.0f)
    , hunger(20.0f)
    , isDead(false)
    , fallDistance(0.0f)
    , lastY(position.y)
    , wasOnGround(false)
    , hungerDamageTimer(0.0f)
    , regenTimer(0.0f)
    , drownTimer(0.0f)
    , isBreaking(false)
    , breakingBlockPos(0)
    , breakingBlockId(0)
    , breakProgress(0.0f)
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

static bool isBlockSolidAt(int wx, int wy, int wz, ChunkManager& chunkManager)
{
  return isBlockSolid(getBlockAtWorld(wx, wy, wz, chunkManager));
}

static void getCollidingBlocks(const AABB& playerAABB, ChunkManager& chunkManager, std::vector<AABB>& outBlocks)
{
  outBlocks.clear();
  
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
        if (isBlockSolidAt(x, y, z, chunkManager))
        {
          outBlocks.push_back(AABB::fromBlockPos(x, y, z));
        }
      }
    }
  }
}

static float moveAxis(
    glm::vec3& position,
    float movement,
    int axis,
    ChunkManager& chunkManager,
    bool& hitSomething)
{
  hitSomething = false;
  
  if (std::abs(movement) < 0.0001f)
    return 0.0f;
  
  float halfWidth = PLAYER_WIDTH * 0.5f;
  float halfDepth = PLAYER_DEPTH * 0.5f;
  
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
  
  getCollidingBlocks(newAABB, chunkManager, s_collisionScratch);
  
  for (const AABB& block : s_collisionScratch)
  {
    if (newAABB.intersects(block))
    {
      hitSomething = true;
      
      if (movement > 0)
      {
        if (axis == 0)
          newPos.x = block.min.x - halfWidth - 0.0001f;
        else if (axis == 1)
          newPos.y = block.min.y - PLAYER_HEIGHT - 0.0001f;
        else
          newPos.z = block.min.z - halfDepth - 0.0001f;
      }
      else
      {
        if (axis == 0)
          newPos.x = block.max.x + halfWidth + 0.0001f;
        else if (axis == 1)
          newPos.y = block.max.y + 0.0001f;
        else
          newPos.z = block.max.z + halfDepth + 0.0001f;
      }
      
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
    position += velocity * dt;
    return;
  }
  
  velocity.y -= GRAVITY * dt;
  
  if (velocity.y < -TERMINAL_VELOCITY)
    velocity.y = -TERMINAL_VELOCITY;
  
  glm::vec3 movement = velocity * dt;
  
  bool hitY = false;
  bool hitX = false;
  bool hitZ = false;
  
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
  
  moveAxis(position, movement.x, 0, chunkManager, hitX);
  if (hitX)
  {
    velocity.x = 0.0f;
  }
  
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
