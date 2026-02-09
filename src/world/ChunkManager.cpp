#include "ChunkManager.h"
#include "../utils/JobSystem.h"
#include "RegionManager.h"
#include "../rendering/Meshing.h"
#include "TerrainGenerator.h"
#include "CaveGenerator.h"
#include <cstring>

bool ChunkManager::hasChunk(int cx, int cy, int cz)
{
  return chunks.find(ChunkCoord(cx, cy, cz)) != chunks.end();
}

bool ChunkManager::isLoading(int cx, int cy, int cz) const
{
  return loadingChunks.find(ChunkCoord(cx, cy, cz)) != loadingChunks.end();
}

bool ChunkManager::isMeshing(int cx, int cy, int cz) const
{
  return meshingChunks.find(ChunkCoord(cx, cy, cz)) != meshingChunks.end();
}

bool ChunkManager::isSaving(int cx, int cy, int cz) const
{
  return savingChunks.find(ChunkCoord(cx, cy, cz)) != savingChunks.end();
}

Chunk *ChunkManager::getChunk(int cx, int cy, int cz)
{
  auto it = chunks.find(ChunkCoord(cx, cy, cz));
  if (it == chunks.end())
    return nullptr;
  return it->second.get();
}

Chunk *ChunkManager::loadChunk(int cx, int cy, int cz)
{
  ChunkCoord key(cx, cy, cz);
  if (savingChunks.count(key) > 0)
    return nullptr;

  if (hasChunk(cx, cy, cz))
    return getChunk(cx, cy, cz);

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
    generateTerrain(c->blocks, cx, cy, cz);
    
    int terrainHeights[CHUNK_SIZE * CHUNK_SIZE];
    getTerrainHeightsForChunk(cx, cz, terrainHeights);
    applyCavesToChunk(*c, DEFAULT_WORLD_SEED, terrainHeights);
  }

  for (int i = 0; i < 6; i++)
  {
    Chunk* neighbor = getChunk(cx + DIRS[i].x, cy + DIRS[i].y, cz + DIRS[i].z);
    if (neighbor != nullptr)
    {
      neighbor->dirtyMesh = true;
    }
  }

  return c;
}

void ChunkManager::unloadChunk(int cx, int cy, int cz)
{
  ChunkCoord key(cx, cy, cz);
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

  ChunkCoord key(cx, cy, cz);
  if (hasChunk(cx, cy, cz) || loadingChunks.count(key) > 0 || savingChunks.count(key) > 0)
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
  ChunkCoord key(cx, cy, cz);
  
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
    std::memcpy(job->blocks, chunk->blocks, CHUNK_VOLUME * sizeof(BlockID));

    jobSystem->enqueueHighPriority(std::move(job));
  }

  chunks.erase(key);
}

void ChunkManager::enqueueMeshChunk(int cx, int cy, int cz)
{
  if (!jobSystem)
    return;

  ChunkCoord key(cx, cy, cz);
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
  std::memcpy(job->blocks, chunk->blocks, CHUNK_VOLUME * sizeof(BlockID));

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

  std::memcpy(job->skyLight, chunk->skyLight, CHUNK_VOLUME * sizeof(uint8_t));

  if (neighborPosX)
    copyNeighborSkyLightFace(job->skyLightPosX, neighborPosX, 0);
  if (neighborNegX)
    copyNeighborSkyLightFace(job->skyLightNegX, neighborNegX, 1);
  if (neighborPosY)
    copyNeighborSkyLightFace(job->skyLightPosY, neighborPosY, 2);
  if (neighborNegY)
    copyNeighborSkyLightFace(job->skyLightNegY, neighborNegY, 3);
  if (neighborPosZ)
    copyNeighborSkyLightFace(job->skyLightPosZ, neighborPosZ, 4);
  if (neighborNegZ)
    copyNeighborSkyLightFace(job->skyLightNegZ, neighborNegZ, 5);

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

void ChunkManager::copyNeighborSkyLightFace(uint8_t* dest, Chunk* neighbor, int face)
{
  switch (face)
  {
    case 0:
      for (int y = 0; y < CHUNK_SIZE; y++)
        for (int z = 0; z < CHUNK_SIZE; z++)
          dest[y * CHUNK_SIZE + z] = neighbor->skyLight[blockIndex(0, y, z)];
      break;
    case 1:
      for (int y = 0; y < CHUNK_SIZE; y++)
        for (int z = 0; z < CHUNK_SIZE; z++)
          dest[y * CHUNK_SIZE + z] = neighbor->skyLight[blockIndex(CHUNK_SIZE - 1, y, z)];
      break;
    case 2:
      for (int x = 0; x < CHUNK_SIZE; x++)
        for (int z = 0; z < CHUNK_SIZE; z++)
          dest[x * CHUNK_SIZE + z] = neighbor->skyLight[blockIndex(x, 0, z)];
      break;
    case 3:
      for (int x = 0; x < CHUNK_SIZE; x++)
        for (int z = 0; z < CHUNK_SIZE; z++)
          dest[x * CHUNK_SIZE + z] = neighbor->skyLight[blockIndex(x, CHUNK_SIZE - 1, z)];
      break;
    case 4:
      for (int x = 0; x < CHUNK_SIZE; x++)
        for (int y = 0; y < CHUNK_SIZE; y++)
          dest[x * CHUNK_SIZE + y] = neighbor->skyLight[blockIndex(x, y, 0)];
      break;
    case 5:
      for (int x = 0; x < CHUNK_SIZE; x++)
        for (int y = 0; y < CHUNK_SIZE; y++)
          dest[x * CHUNK_SIZE + y] = neighbor->skyLight[blockIndex(x, y, CHUNK_SIZE - 1)];
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

  auto completedSaves = jobSystem->pollCompletedSaves();
  for (auto& job : completedSaves)
  {
    savingChunks.erase(ChunkCoord(job->cx, job->cy, job->cz));
  }
}

void ChunkManager::onGenerateComplete(GenerateChunkJob* job)
{
  ChunkCoord key(job->cx, job->cy, job->cz);
  loadingChunks.erase(key);

  if (hasChunk(job->cx, job->cy, job->cz))
    return;

  auto [it, inserted] = chunks.emplace(key, std::make_unique<Chunk>());
  Chunk* c = it->second.get();
  c->position = {job->cx, job->cy, job->cz};

  std::memcpy(c->blocks, job->blocks, CHUNK_VOLUME * sizeof(BlockID));
  std::memcpy(c->skyLight, job->skyLight, CHUNK_VOLUME * sizeof(uint8_t));

  glGenVertexArrays(1, &c->vao);
  glGenBuffers(1, &c->vbo);
  glGenBuffers(1, &c->ebo);

  c->dirtyMesh = true;

  for (int i = 0; i < 6; i++)
  {
    Chunk* neighbor = getChunk(job->cx + DIRS[i].x, job->cy + DIRS[i].y, job->cz + DIRS[i].z);
    if (neighbor != nullptr)
    {
      neighbor->dirtyMesh = true;
    }
  }
}

void ChunkManager::onMeshComplete(MeshChunkJob* job)
{
  meshingChunks.erase(ChunkCoord(job->cx, job->cy, job->cz));

  Chunk* chunk = getChunk(job->cx, job->cy, job->cz);
  if (!chunk)
    return;

  uploadToGPU(*chunk, job->vertices, job->indices);
  uploadWaterToGPU(*chunk, job->waterVertices, job->waterIndices);
  chunk->dirtyMesh = false;
}
