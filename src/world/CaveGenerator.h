#pragma once
#include <cstdint>
#include <glm/glm.hpp>
#include "Chunk.h"

// Default world seed used for cave generation; adjust to change world layout.
constexpr uint32_t DEFAULT_WORLD_SEED = 1488u;

struct CaveConfig
{
  // Cheese cave parameters (large open caverns)
  float cheeseFrequency    = 0.015f;  // Lower = bigger caves
  float cheeseThreshold    = -0.3f;   // Higher = more caves (negative means carve where noise is low)
  int   cheeseOctaves      = 2;
  float cheeseGain         = 0.5f;
  
  // Spaghetti tunnels connecting cheese caves
  float spaghettiFrequency = 0.04f;   // Tunnel wiggliness
  float spaghettiThreshold = 0.08f;   // Tunnel width
  float spaghettiYStretch  = 0.6f;    // Horizontal bias
  
  // Depth-based settings
  float minCaveHeight      = 5.0f;    // Absolute minimum Y for caves
  float surfaceMargin      = 6.0f;    // Stay this many blocks below terrain surface
  
  // Vegetation (unused for now)
  float vegFrequency       = 0.08f;
  float vegThreshold       = 0.25f;
};

// Returns cave density at world position; >0 means carve to air.
// Also needs terrain height at this XZ to avoid breaking surface.
float caveDensity(int wx, int wy, int wz, int terrainHeight, uint32_t seed, const CaveConfig& cfg = CaveConfig{});

// Optional vegetation mask helper for future use.
bool caveVegetationMask(int wx, int wy, int wz, uint32_t seed, const CaveConfig& cfg = CaveConfig{});

// Carve caves into a raw block buffer. Needs terrain heights to avoid surface.
void applyCavesToBlocks(BlockID* blocks, const glm::ivec3& chunkPos, uint32_t worldSeed, 
                        const int* terrainHeights, const CaveConfig& cfg = CaveConfig{}, 
                        bool* outVegetationMask = nullptr);

// Convenience wrapper for chunk instances.
void applyCavesToChunk(Chunk& c, uint32_t worldSeed, const int* terrainHeights, 
                       const CaveConfig& cfg = CaveConfig{}, bool* outVegetationMask = nullptr);
