#include "ChunkManager.h"
#include "JobSystem.h"
#include "RegionManager.h"
#include "Meshing.h"
#include "PerlinNoise.hpp"
#include <cmath>
#include <cstring>

static const siv::PerlinNoise::seed_type TERRAIN_SEED = 69420;
static const siv::PerlinNoise perlin{TERRAIN_SEED};
static const siv::PerlinNoise perlinDetail{TERRAIN_SEED + 1};
static const siv::PerlinNoise perlinTrees{TERRAIN_SEED + 2};

constexpr int BASE_HEIGHT = 32;
constexpr int HEIGHT_VARIATION = 28;
constexpr int DIRT_DEPTH = 5;

constexpr int TREE_TRUNK_HEIGHT = 5;
constexpr int TREE_LEAF_RADIUS = 2;

constexpr uint8_t BLOCK_AIR = 0;
constexpr uint8_t BLOCK_DIRT = 1;
constexpr uint8_t BLOCK_GRASS = 2;
constexpr uint8_t BLOCK_STONE = 3;
constexpr uint8_t BLOCK_LOG = 5;
constexpr uint8_t BLOCK_LEAVES = 6;

constexpr int TREE_GRID_SIZE = 7;
constexpr int TREE_OFFSET_RANGE = 10;
constexpr float TREE_SPAWN_CHANCE = 0.2f;

static bool shouldPlaceTree(int worldX, int worldZ)
{
  int cellX = worldX >= 0 ? worldX / TREE_GRID_SIZE : (worldX - TREE_GRID_SIZE + 1) / TREE_GRID_SIZE;
  int cellZ = worldZ >= 0 ? worldZ / TREE_GRID_SIZE : (worldZ - TREE_GRID_SIZE + 1) / TREE_GRID_SIZE;
  
  unsigned int cellHash = static_cast<unsigned int>(cellX * 73856093) ^ 
                          static_cast<unsigned int>(cellZ * 19349663);
  
  float spawnChance = (cellHash % 10000) / 10000.0f;
  if (spawnChance >= TREE_SPAWN_CHANCE)
    return false;
  
  unsigned int offsetHash = cellHash * 31337;
  int offsetX = static_cast<int>(offsetHash % TREE_OFFSET_RANGE);
  int offsetZ = static_cast<int>((offsetHash / TREE_OFFSET_RANGE) % TREE_OFFSET_RANGE);
  
  int treePosX = cellX * TREE_GRID_SIZE + offsetX;
  int treePosZ = cellZ * TREE_GRID_SIZE + offsetZ;
  
  return worldX == treePosX && worldZ == treePosZ;
}

static double getTerrainHeight(float worldX, float worldZ)
{
  double continentNoise = perlin.octave2D_01(
      worldX * 0.002,
      worldZ * 0.002,
      2,
      0.5
  );
  
  continentNoise = std::pow(continentNoise, 1.2);
  
  double hillNoise = perlin.octave2D_01(
      worldX * 0.01,
      worldZ * 0.01,
      4,
      0.45
  );
  
  double detailNoise = perlinDetail.octave2D_01(
      worldX * 0.05,
      worldZ * 0.05,
      2,
      0.5
  );
  
  double blendedNoise = continentNoise * 0.4 + hillNoise * 0.5 + detailNoise * 0.1;
  
  blendedNoise = blendedNoise * blendedNoise * (3.0 - 2.0 * blendedNoise);
  
  return BASE_HEIGHT + blendedNoise * HEIGHT_VARIATION;
}

bool ChunkManager::hasChunk(int cx, int cy, int cz)
{
  ChunkCoord key{cx, cy, cz};
  return chunks.find(key) != chunks.end();
}

bool ChunkManager::isLoading(int cx, int cy, int cz) const
{
  ChunkCoord key{cx, cy, cz};
  return loadingChunks.find(key) != loadingChunks.end();
}

bool ChunkManager::isMeshing(int cx, int cy, int cz) const
{
  ChunkCoord key{cx, cy, cz};
  return meshingChunks.find(key) != meshingChunks.end();
}

bool ChunkManager::isSaving(int cx, int cy, int cz) const
{
  ChunkCoord key{cx, cy, cz};
  return savingChunks.find(key) != savingChunks.end();
}

namespace
{
  void setBlockIfInChunk(Chunk &c, int localX, int localY, int localZ, uint8_t blockId, bool overwriteSolid = false)
  {
    if (localX < 0 || localX >= CHUNK_SIZE ||
        localY < 0 || localY >= CHUNK_SIZE ||
        localZ < 0 || localZ >= CHUNK_SIZE)
      return;
    
    int idx = blockIndex(localX, localY, localZ);
    if (overwriteSolid || c.blocks[idx] == BLOCK_AIR)
    {
      c.blocks[idx] = blockId;
    }
  }

  void generateTerrain(Chunk &c)
  {
    int worldOffsetX = c.position.x * CHUNK_SIZE;
    int worldOffsetY = c.position.y * CHUNK_SIZE;
    int worldOffsetZ = c.position.z * CHUNK_SIZE;

    for (int x = 0; x < CHUNK_SIZE; x++)
    {
      for (int z = 0; z < CHUNK_SIZE; z++)
      {
        float worldX = static_cast<float>(worldOffsetX + x);
        float worldZ = static_cast<float>(worldOffsetZ + z);

        int terrainHeight = static_cast<int>(std::round(getTerrainHeight(worldX, worldZ)));

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

    for (int x = -TREE_LEAF_RADIUS; x < CHUNK_SIZE + TREE_LEAF_RADIUS; x++)
    {
      for (int z = -TREE_LEAF_RADIUS; z < CHUNK_SIZE + TREE_LEAF_RADIUS; z++)
      {
        int worldX = worldOffsetX + x;
        int worldZ = worldOffsetZ + z;
        
        if (!shouldPlaceTree(worldX, worldZ))
          continue;
        
        int terrainHeight = static_cast<int>(std::round(getTerrainHeight(
            static_cast<float>(worldX), static_cast<float>(worldZ))));
        
        int treeBaseY = terrainHeight + 1;
        
        for (int ty = 0; ty < TREE_TRUNK_HEIGHT; ty++)
        {
          int localX = x;
          int localY = treeBaseY + ty - worldOffsetY;
          int localZ = z;
          setBlockIfInChunk(c, localX, localY, localZ, BLOCK_LOG, true);
        }
        
        int leafCenterY = treeBaseY + TREE_TRUNK_HEIGHT - 1;
        for (int lx = -TREE_LEAF_RADIUS; lx <= TREE_LEAF_RADIUS; lx++)
        {
          for (int ly = -1; ly <= TREE_LEAF_RADIUS; ly++)
          {
            for (int lz = -TREE_LEAF_RADIUS; lz <= TREE_LEAF_RADIUS; lz++)
            {
              int dist = std::abs(lx) + std::abs(ly) + std::abs(lz);
              if (dist > TREE_LEAF_RADIUS + 1)
                continue;
              
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

  ChunkCoord key{cx, cy, cz};
  auto [it, inserted] = chunks.emplace(key, std::make_unique<Chunk>());
  (void)inserted;
  Chunk *c = it->second.get();
  c->position = {cx, cy, cz};

  glGenVertexArrays(1, &c->vao);
  glGenBuffers(1, &c->vbo);
  glGenBuffers(1, &c->ebo);

  bool loadedFromDisk = false;
  if (regionManager)
  {
    loadedFromDisk = regionManager->loadChunkData(cx, cy, cz, c->blocks);
  }

  if (!loadedFromDisk)
  {
    generateTerrain(*c);
  }

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
    if (regionManager)
    {
      regionManager->saveChunkData(cx, cy, cz, it->second->blocks);
    }
    chunks.erase(it);
  }
}

void ChunkManager::enqueueLoadChunk(int cx, int cy, int cz)
{
  if (!jobSystem)
  {
    loadChunk(cx, cy, cz);
    return;
  }

  ChunkCoord key{cx, cy, cz};
  if (hasChunk(cx, cy, cz) || loadingChunks.count(key) > 0)
    return;

  loadingChunks.insert(key);

  auto job = std::make_unique<GenerateChunkJob>();
  job->cx = cx;
  job->cy = cy;
  job->cz = cz;

  jobSystem->enqueue(std::move(job));
}

void ChunkManager::enqueueSaveAndUnload(int cx, int cy, int cz)
{
  ChunkCoord key{cx, cy, cz};
  
  if (!hasChunk(cx, cy, cz))
    return;

  if (savingChunks.count(key) > 0)
    return;

  Chunk* chunk = getChunk(cx, cy, cz);
  if (!chunk)
    return;

  if (jobSystem && regionManager)
  {
    savingChunks.insert(key);

    auto job = std::make_unique<SaveChunkJob>();
    job->cx = cx;
    job->cy = cy;
    job->cz = cz;
    std::memcpy(job->blocks, chunk->blocks, CHUNK_VOLUME);

    jobSystem->enqueue(std::move(job));
  }

  chunks.erase(key);
}

void ChunkManager::enqueueMeshChunk(int cx, int cy, int cz)
{
  if (!jobSystem)
    return;

  ChunkCoord key{cx, cy, cz};
  if (meshingChunks.count(key) > 0)
    return;

  Chunk* chunk = getChunk(cx, cy, cz);
  if (!chunk)
    return;

  meshingChunks.insert(key);

  auto job = std::make_unique<MeshChunkJob>();
  job->cx = cx;
  job->cy = cy;
  job->cz = cz;
  std::memcpy(job->blocks, chunk->blocks, CHUNK_VOLUME);

  Chunk* neighborPosX = getChunk(cx + 1, cy, cz);
  Chunk* neighborNegX = getChunk(cx - 1, cy, cz);
  Chunk* neighborPosY = getChunk(cx, cy + 1, cz);
  Chunk* neighborNegY = getChunk(cx, cy - 1, cz);
  Chunk* neighborPosZ = getChunk(cx, cy, cz + 1);
  Chunk* neighborNegZ = getChunk(cx, cy, cz - 1);

  if (neighborPosX)
  {
    job->hasNeighborPosX = true;
    copyNeighborFace(job->neighborPosX, neighborPosX, 0);
  }
  if (neighborNegX)
  {
    job->hasNeighborNegX = true;
    copyNeighborFace(job->neighborNegX, neighborNegX, 1);
  }
  if (neighborPosY)
  {
    job->hasNeighborPosY = true;
    copyNeighborFace(job->neighborPosY, neighborPosY, 2);
  }
  if (neighborNegY)
  {
    job->hasNeighborNegY = true;
    copyNeighborFace(job->neighborNegY, neighborNegY, 3);
  }
  if (neighborPosZ)
  {
    job->hasNeighborPosZ = true;
    copyNeighborFace(job->neighborPosZ, neighborPosZ, 4);
  }
  if (neighborNegZ)
  {
    job->hasNeighborNegZ = true;
    copyNeighborFace(job->neighborNegZ, neighborNegZ, 5);
  }

  jobSystem->enqueue(std::move(job));
}

void ChunkManager::copyNeighborFace(BlockID* dest, Chunk* neighbor, int face)
{
  switch (face)
  {
    case 0:
      for (int y = 0; y < CHUNK_SIZE; y++)
        for (int z = 0; z < CHUNK_SIZE; z++)
          dest[y * CHUNK_SIZE + z] = neighbor->blocks[blockIndex(0, y, z)];
      break;
    case 1:
      for (int y = 0; y < CHUNK_SIZE; y++)
        for (int z = 0; z < CHUNK_SIZE; z++)
          dest[y * CHUNK_SIZE + z] = neighbor->blocks[blockIndex(CHUNK_SIZE - 1, y, z)];
      break;
    case 2:
      for (int x = 0; x < CHUNK_SIZE; x++)
        for (int z = 0; z < CHUNK_SIZE; z++)
          dest[x * CHUNK_SIZE + z] = neighbor->blocks[blockIndex(x, 0, z)];
      break;
    case 3:
      for (int x = 0; x < CHUNK_SIZE; x++)
        for (int z = 0; z < CHUNK_SIZE; z++)
          dest[x * CHUNK_SIZE + z] = neighbor->blocks[blockIndex(x, CHUNK_SIZE - 1, z)];
      break;
    case 4:
      for (int x = 0; x < CHUNK_SIZE; x++)
        for (int y = 0; y < CHUNK_SIZE; y++)
          dest[x * CHUNK_SIZE + y] = neighbor->blocks[blockIndex(x, y, 0)];
      break;
    case 5:
      for (int x = 0; x < CHUNK_SIZE; x++)
        for (int y = 0; y < CHUNK_SIZE; y++)
          dest[x * CHUNK_SIZE + y] = neighbor->blocks[blockIndex(x, y, CHUNK_SIZE - 1)];
      break;
  }
}

void ChunkManager::update()
{
  if (!jobSystem)
    return;

  auto completedGenerations = jobSystem->pollCompletedGenerations();
  for (auto& job : completedGenerations)
  {
    onGenerateComplete(job.get());
  }

  auto completedMeshes = jobSystem->pollCompletedMeshes();
  for (auto& job : completedMeshes)
  {
    onMeshComplete(job.get());
  }
}

void ChunkManager::onGenerateComplete(GenerateChunkJob* job)
{
  ChunkCoord key{job->cx, job->cy, job->cz};
  loadingChunks.erase(key);

  if (hasChunk(job->cx, job->cy, job->cz))
    return;

  auto [it, inserted] = chunks.emplace(key, std::make_unique<Chunk>());
  Chunk* c = it->second.get();
  c->position = {job->cx, job->cy, job->cz};

  std::memcpy(c->blocks, job->blocks, CHUNK_VOLUME);
  std::memcpy(c->skyLight, job->skyLight, CHUNK_VOLUME);

  glGenVertexArrays(1, &c->vao);
  glGenBuffers(1, &c->vbo);
  glGenBuffers(1, &c->ebo);

  c->dirtyMesh = true;

  const int neighborOffsets[6][3] = {
    {1, 0, 0}, {-1, 0, 0},
    {0, 1, 0}, {0, -1, 0},
    {0, 0, 1}, {0, 0, -1}
  };

  for (const auto& offset : neighborOffsets)
  {
    Chunk* neighbor = getChunk(job->cx + offset[0], job->cy + offset[1], job->cz + offset[2]);
    if (neighbor != nullptr)
    {
      neighbor->dirtyMesh = true;
    }
  }
}

void ChunkManager::onMeshComplete(MeshChunkJob* job)
{
  ChunkCoord key{job->cx, job->cy, job->cz};
  meshingChunks.erase(key);

  Chunk* chunk = getChunk(job->cx, job->cy, job->cz);
  if (!chunk)
    return;

  uploadToGPU(*chunk, job->vertices, job->indices);
  chunk->dirtyMesh = false;
}
