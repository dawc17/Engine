#pragma once
#include "glm/fwd.hpp"
#include <cstdint>
#include <glm/glm.hpp>

#include "Inventory.h"

struct ChunkManager;

// Minecraft-style player dimensions
constexpr float PLAYER_WIDTH = 0.6f;
constexpr float PLAYER_DEPTH = 0.6f;
constexpr float PLAYER_HEIGHT = 1.8f;
constexpr float PLAYER_EYE_HEIGHT = 1.62f;  // Eyes are slightly below top

// Physics constants
constexpr float GRAVITY = 28.0f;           // Blocks per second squared
constexpr float JUMP_VELOCITY = 9.0f;      // Initial upward velocity when jumping
constexpr float TERMINAL_VELOCITY = 78.0f; // Max fall speed

struct AABB
{
  glm::vec3 min;  // Bottom-southwest corner
  glm::vec3 max;  // Top-northeast corner

  bool intersects(const AABB& other) const;
  static AABB fromBlockPos(int x, int y, int z);
};

enum class Gamemode
{
  Survival = 0,
  Creative = 1
};

struct Player
{
  glm::vec3 position;  // Feet position (bottom-center of AABB)
  glm::vec3 velocity;
  
  float yaw;
  float pitch;
  
  bool onGround;
  bool noclip;  
  Gamemode gamemode;

  float health;
  float hunger;
  bool isDead;
  float fallDistance;
  float lastY;
  bool wasOnGround;
  float hungerDamageTimer;
  float regenTimer;
  float drownTimer;

  bool isBreaking;
  glm::ivec3 breakingBlockPos;
  uint8_t breakingBlockId;
  float breakProgress;

  Inventory inventory;

  Player();

  // Get the AABB for collision detection
  AABB getAABB() const;

  // Get eye position for camera
  glm::vec3 getEyePosition() const;

  // Apply movement input (WASD)
  void applyMovement(const glm::vec3& inputDir, float speed);

  // Apply physics (gravity, collision) and move
  void update(float dt, ChunkManager& chunkManager);

  // Try to jump (only if on ground)
  void jump();
};

// Get forward direction on XZ plane (for movement)
glm::vec3 playerForwardXZ(const Player& player);

// Get right direction on XZ plane (for strafing)
glm::vec3 playerRightXZ(const Player& player);

