#include "ChunkManager.h"
#include "PerlinNoise.hpp"
#include <iostream>
#include <cmath>

// Global Perlin noise generators with different seeds for varied terrain
static const siv::PerlinNoise::seed_type TERRAIN_SEED = 12345u;
static const siv::PerlinNoise perlin{TERRAIN_SEED};
static const siv::PerlinNoise perlinDetail{TERRAIN_SEED + 1};
static const siv::PerlinNoise perlinTrees{TERRAIN_SEED + 2};

// Terrain generation parameters
constexpr int BASE_HEIGHT = 32;            // Sea level / minimum terrain height
constexpr int HEIGHT_VARIATION = 28;       // Max height added by noise
constexpr int DIRT_DEPTH = 5;              // Thicker dirt/grass layer

// Tree parameters
constexpr int TREE_TRUNK_HEIGHT = 5;       // Height of tree trunk
constexpr int TREE_LEAF_RADIUS = 2;        // Radius of leaf canopy

// Block IDs
constexpr uint8_t BLOCK_AIR = 0;
constexpr uint8_t BLOCK_DIRT = 1;
constexpr uint8_t BLOCK_GRASS = 2;
constexpr uint8_t BLOCK_STONE = 3;
constexpr uint8_t BLOCK_LOG = 5;
constexpr uint8_t BLOCK_LEAVES = 6;

// Determine if a tree should be placed at this world position
static bool shouldPlaceTree(int worldX, int worldZ)
{
  // Use noise to create clustered tree distribution
  double treeNoise = perlinTrees.octave2D_01(worldX * 0.05, worldZ * 0.05, 2);
  
  // Use a hash function to make placement deterministic but pseudo-random
  unsigned int hash = static_cast<unsigned int>(worldX * 73856093) ^ 
                      static_cast<unsigned int>(worldZ * 19349663);
  float random = (hash % 10000) / 10000.0f;
  
  // Trees spawn where noise is high and random value is below threshold
  // Higher noise threshold + lower random chance = sparser trees
  return treeNoise > 0.65 && random < 0.03f;
}

// Hybrid FBM terrain function - produces smoother, more natural terrain
static double getTerrainHeight(float worldX, float worldZ)
{
  // Large-scale continental shapes (very smooth)
  double continentNoise = perlin.octave2D_01(
      worldX * 0.002,
      worldZ * 0.002,
      2,      // Few octaves for smooth base
      0.5     // Standard persistence
  );
  
  // Apply smoothing bias - push values away from middle for flatter areas
  // This creates more defined plains and hills
  continentNoise = std::pow(continentNoise, 1.2);
  
  // Medium-scale hills
  double hillNoise = perlin.octave2D_01(
      worldX * 0.01,
      worldZ * 0.01,
      4,      // More octaves for detail
      0.45    // Slightly lower persistence for smoother hills
  );
  
  // Small-scale detail variation
  double detailNoise = perlinDetail.octave2D_01(
      worldX * 0.05,
      worldZ * 0.05,
      2,
      0.5
  );
  
  // Blend the noise layers with weights
  // Continental (40%) + Hills (50%) + Detail (10%)
  double blendedNoise = continentNoise * 0.4 + hillNoise * 0.5 + detailNoise * 0.1;
  
  // Apply final smoothing curve - reduces terracing
  blendedNoise = blendedNoise * blendedNoise * (3.0 - 2.0 * blendedNoise); // Smoothstep
  
  return BASE_HEIGHT + blendedNoise * HEIGHT_VARIATION;
}

bool ChunkManager::hasChunk(int cx, int cy, int cz)
{
  ChunkCoord key{cx, cy, cz};
  return chunks.find(key) != chunks.end();
}

namespace
{
  // Helper to set a block if within chunk bounds
  void setBlockIfInChunk(Chunk &c, int localX, int localY, int localZ, uint8_t blockId, bool overwriteSolid = false)
  {
    if (localX < 0 || localX >= CHUNK_SIZE ||
        localY < 0 || localY >= CHUNK_SIZE ||
        localZ < 0 || localZ >= CHUNK_SIZE)
      return;
    
    int idx = blockIndex(localX, localY, localZ);
    // Only overwrite air unless overwriteSolid is true
    if (overwriteSolid || c.blocks[idx] == BLOCK_AIR)
    {
      c.blocks[idx] = blockId;
    }
  }

  void generateTerrain(Chunk &c)
  {
    // World position offset for this chunk
    int worldOffsetX = c.position.x * CHUNK_SIZE;
    int worldOffsetY = c.position.y * CHUNK_SIZE;
    int worldOffsetZ = c.position.z * CHUNK_SIZE;

    // First pass: generate base terrain
    for (int x = 0; x < CHUNK_SIZE; x++)
    {
      for (int z = 0; z < CHUNK_SIZE; z++)
      {
        // Calculate world coordinates
        float worldX = static_cast<float>(worldOffsetX + x);
        float worldZ = static_cast<float>(worldOffsetZ + z);

        // Get smooth terrain height using hybrid FBM
        int terrainHeight = static_cast<int>(std::round(getTerrainHeight(worldX, worldZ)));

        // Fill the column based on world Y coordinates
        for (int y = 0; y < CHUNK_SIZE; y++)
        {
          int worldY = worldOffsetY + y;
          int i = blockIndex(x, y, z);

          if (worldY > terrainHeight)
          {
            c.blocks[i] = BLOCK_AIR;
          }
          else if (worldY == terrainHeight)
          {
            c.blocks[i] = BLOCK_GRASS;
          }
          else if (worldY > terrainHeight - DIRT_DEPTH)
          {
            c.blocks[i] = BLOCK_DIRT;
          }
          else
          {
            c.blocks[i] = BLOCK_STONE;
          }
        }
      }
    }

    // Second pass: add trees
    // Check a wider area to handle trees from neighboring chunks that extend into this one
    for (int x = -TREE_LEAF_RADIUS; x < CHUNK_SIZE + TREE_LEAF_RADIUS; x++)
    {
      for (int z = -TREE_LEAF_RADIUS; z < CHUNK_SIZE + TREE_LEAF_RADIUS; z++)
      {
        int worldX = worldOffsetX + x;
        int worldZ = worldOffsetZ + z;
        
        if (!shouldPlaceTree(worldX, worldZ))
          continue;
        
        // Get terrain height at tree position
        int terrainHeight = static_cast<int>(std::round(getTerrainHeight(
            static_cast<float>(worldX), static_cast<float>(worldZ))));
        
        int treeBaseY = terrainHeight + 1;  // Tree starts on top of grass
        
        // Generate trunk
        for (int ty = 0; ty < TREE_TRUNK_HEIGHT; ty++)
        {
          int localX = x;
          int localY = treeBaseY + ty - worldOffsetY;
          int localZ = z;
          setBlockIfInChunk(c, localX, localY, localZ, BLOCK_LOG, true);
        }
        
        // Generate leaves (sphere-ish shape at top of trunk)
        int leafCenterY = treeBaseY + TREE_TRUNK_HEIGHT - 1;
        for (int lx = -TREE_LEAF_RADIUS; lx <= TREE_LEAF_RADIUS; lx++)
        {
          for (int ly = -1; ly <= TREE_LEAF_RADIUS; ly++)
          {
            for (int lz = -TREE_LEAF_RADIUS; lz <= TREE_LEAF_RADIUS; lz++)
            {
              // Skip corners for rounder shape
              int dist = std::abs(lx) + std::abs(ly) + std::abs(lz);
              if (dist > TREE_LEAF_RADIUS + 1)
                continue;
              
              // Don't place leaves where trunk is (except at very top)
              if (lx == 0 && lz == 0 && ly < TREE_LEAF_RADIUS)
                continue;
              
              int localX = x + lx;
              int localY = leafCenterY + ly - worldOffsetY;
              int localZ = z + lz;
              setBlockIfInChunk(c, localX, localY, localZ, BLOCK_LEAVES);
            }
          }
        }
      }
    }
  }
}

Chunk *ChunkManager::getChunk(int cx, int cy, int cz)
{
  ChunkCoord key{cx, cy, cz};
  auto it = chunks.find(key);
  if (it == chunks.end())
    return nullptr;
  return it->second.get();
}

Chunk *ChunkManager::loadChunk(int cx, int cy, int cz)
{
  if (hasChunk(cx, cy, cz))
    return getChunk(cx, cy, cz);

  std::cout << "Loading chunk at (" << cx << ", " << cy << ", " << cz << ")" << std::endl;

  ChunkCoord key{cx, cy, cz};
  auto [it, inserted] = chunks.emplace(key, std::make_unique<Chunk>());
  (void)inserted;
  Chunk *c = it->second.get();
  c->position = {cx, cy, cz};

  glGenVertexArrays(1, &c->vao);
  glGenBuffers(1, &c->vbo);
  glGenBuffers(1, &c->ebo);

  generateTerrain(*c);

  // Mark neighboring chunks as dirty so they rebuild their meshes
  // This ensures faces at chunk boundaries are properly culled
  const int neighborOffsets[6][3] = {
    {1, 0, 0}, {-1, 0, 0},
    {0, 1, 0}, {0, -1, 0},
    {0, 0, 1}, {0, 0, -1}
  };
  
  for (const auto& offset : neighborOffsets)
  {
    Chunk* neighbor = getChunk(cx + offset[0], cy + offset[1], cz + offset[2]);
    if (neighbor != nullptr)
    {
      neighbor->dirtyMesh = true;
    }
  }

  return c;
}

void ChunkManager::unloadChunk(int cx, int cy, int cz)
{
  ChunkCoord key{cx, cy, cz};
  auto it = chunks.find(key);

  if (it != chunks.end())
  {
    chunks.erase(it);
  }
}
