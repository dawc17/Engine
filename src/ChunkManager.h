#pragma once
#include "Chunk.h"
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <unordered_map>
#include <unordered_set>

class JobSystem;
class RegionManager;
struct GenerateChunkJob;
struct MeshChunkJob;

struct ChunkManager
{
  struct ChunkCoord
  {
    int x;
    int y;
    int z;

    bool operator==(const ChunkCoord &other) const noexcept
    {
      return x == other.x && y == other.y && z == other.z;
    }
  };

  struct ChunkCoordHash
  {
    std::size_t operator()(const ChunkCoord &coord) const noexcept
    {
      std::size_t h = std::hash<int>{}(coord.x);
      h ^= std::hash<int>{}(coord.y) + 0x9e3779b9 + (h << 6) + (h >> 2);
      h ^= std::hash<int>{}(coord.z) + 0x9e3779b9 + (h << 6) + (h >> 2);
      return h;
    }
  };

  using ChunkMap = std::unordered_map<ChunkCoord, std::unique_ptr<Chunk>, ChunkCoordHash>;
  using ChunkSet = std::unordered_set<ChunkCoord, ChunkCoordHash>;

  ChunkMap chunks;
  ChunkSet loadingChunks;
  ChunkSet meshingChunks;
  ChunkSet savingChunks;

  JobSystem* jobSystem = nullptr;
  RegionManager* regionManager = nullptr;

  void setJobSystem(JobSystem* js) { jobSystem = js; }
  void setRegionManager(RegionManager* rm) { regionManager = rm; }

  Chunk *getChunk(int cx, int cy, int cz);
  bool hasChunk(int cx, int cy, int cz);

  Chunk *loadChunk(int cx, int cy, int cz);
  void unloadChunk(int cx, int cy, int cz);

  void enqueueLoadChunk(int cx, int cy, int cz);
  void enqueueSaveAndUnload(int cx, int cy, int cz);
  void enqueueMeshChunk(int cx, int cy, int cz);

  bool isLoading(int cx, int cy, int cz) const;
  bool isMeshing(int cx, int cy, int cz) const;
  bool isSaving(int cx, int cy, int cz) const;

  void update();

  void onGenerateComplete(GenerateChunkJob* job);
  void onMeshComplete(MeshChunkJob* job);

private:
  void copyNeighborFace(BlockID* dest, Chunk* neighbor, int face);
};
