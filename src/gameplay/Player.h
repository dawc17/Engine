#pragma once
#include "glm/fwd.hpp"
#include <cstdint>
#include <glm/glm.hpp>

#include "Inventory.h"

struct ChunkManager;

constexpr float PLAYER_WIDTH = 0.6f;
constexpr float PLAYER_DEPTH = 0.6f;
constexpr float PLAYER_HEIGHT = 1.8f;
constexpr float PLAYER_EYE_HEIGHT = 1.62f;

constexpr float GRAVITY = 28.0f;
constexpr float JUMP_VELOCITY = 9.0f;
constexpr float TERMINAL_VELOCITY = 78.0f;

struct AABB
{
  glm::vec3 min;
  glm::vec3 max;

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
  glm::vec3 position;
  glm::vec3 velocity;
  
  float yaw;
  float pitch;
  
  bool onGround;
  bool noclip;
  bool flying;
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

  bool isSwinging;
  float swingProgress;
  bool isPlacing;
  float placeProgress;
  
  Inventory inventory;

  Player();

  AABB getAABB() const;

  glm::vec3 getEyePosition() const;

  void applyMovement(const glm::vec3& inputDir, float speed);

  void update(float dt, ChunkManager& chunkManager);

  void jump();
};

glm::vec3 playerForwardXZ(const Player& player);

glm::vec3 playerRightXZ(const Player& player);

