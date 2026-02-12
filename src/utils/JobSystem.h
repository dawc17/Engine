#pragma once
#include "../world/Chunk.h"
#include "../rendering/Meshing.h"
#include "../world/RegionManager.h"
#include <functional>
#include <memory>
#include <queue>
#include <vector>
#ifndef __EMSCRIPTEN__
#include <atomic>
#include <condition_variable>
#include <mutex>
#include <thread>
#endif

enum class JobType
{
    Generate,
    Mesh,
    Save
};

struct Job
{
    JobType type;
    int cx, cy, cz;

    virtual ~Job() = default;
};

struct GenerateChunkJob : Job
{
    BlockID blocks[CHUNK_VOLUME];
    uint8_t skyLight[CHUNK_VOLUME];
    bool loadedFromDisk;

    GenerateChunkJob()
    {
        type = JobType::Generate;
        loadedFromDisk = false;
    }
};

struct MeshChunkJob : Job
{
    BlockID blocks[CHUNK_VOLUME];
    BlockID neighborPosX[CHUNK_SIZE * CHUNK_SIZE];
    BlockID neighborNegX[CHUNK_SIZE * CHUNK_SIZE];
    BlockID neighborPosY[CHUNK_SIZE * CHUNK_SIZE];
    BlockID neighborNegY[CHUNK_SIZE * CHUNK_SIZE];
    BlockID neighborPosZ[CHUNK_SIZE * CHUNK_SIZE];
    BlockID neighborNegZ[CHUNK_SIZE * CHUNK_SIZE];
    bool hasNeighborPosX, hasNeighborNegX;
    bool hasNeighborPosY, hasNeighborNegY;
    bool hasNeighborPosZ, hasNeighborNegZ;

    uint8_t skyLight[CHUNK_VOLUME];
    uint8_t skyLightPosX[CHUNK_SIZE * CHUNK_SIZE];
    uint8_t skyLightNegX[CHUNK_SIZE * CHUNK_SIZE];
    uint8_t skyLightPosY[CHUNK_SIZE * CHUNK_SIZE];
    uint8_t skyLightNegY[CHUNK_SIZE * CHUNK_SIZE];
    uint8_t skyLightPosZ[CHUNK_SIZE * CHUNK_SIZE];
    uint8_t skyLightNegZ[CHUNK_SIZE * CHUNK_SIZE];

    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;
    std::vector<Vertex> waterVertices;
    std::vector<uint32_t> waterIndices;

    MeshChunkJob()
    {
        type = JobType::Mesh;
        hasNeighborPosX = hasNeighborNegX = false;
        hasNeighborPosY = hasNeighborNegY = false;
        hasNeighborPosZ = hasNeighborNegZ = false;
    }
};

struct SaveChunkJob : Job
{
    BlockID blocks[CHUNK_VOLUME];

    SaveChunkJob()
    {
        type = JobType::Save;
    }
};

struct ChunkManager;

class JobSystem
{
public:
    JobSystem();
    ~JobSystem();

    void start(int numWorkers = 4);
    void stop();

    void setRegionManager(RegionManager* rm) { regionManager = rm; }
    void setChunkManager(ChunkManager* cm) { chunkManager = cm; }

    void enqueue(std::unique_ptr<Job> job);
    void enqueueHighPriority(std::unique_ptr<Job> job);

    std::vector<std::unique_ptr<GenerateChunkJob>> pollCompletedGenerations();
    std::vector<std::unique_ptr<MeshChunkJob>> pollCompletedMeshes();
    std::vector<std::unique_ptr<SaveChunkJob>> pollCompletedSaves();

    bool hasCompletedWork() const;
    size_t pendingJobCount() const;

private:
#ifndef __EMSCRIPTEN__
    std::vector<std::thread> workers;
    std::mutex queueMutex;
    std::condition_variable condition;
    std::atomic<bool> running;
    std::mutex completedMutex;
#else
    bool running = false;
#endif
    std::queue<std::unique_ptr<Job>> jobQueue;
    std::queue<std::unique_ptr<Job>> highPriorityQueue;

    std::vector<std::unique_ptr<GenerateChunkJob>> completedGenerations;
    std::vector<std::unique_ptr<MeshChunkJob>> completedMeshes;
    std::vector<std::unique_ptr<SaveChunkJob>> completedSaves;

    RegionManager* regionManager;
    ChunkManager* chunkManager;

#ifndef __EMSCRIPTEN__
    void workerLoop();
#endif
    void processJob(std::unique_ptr<Job> job);
    void processGenerateJob(GenerateChunkJob* job);
    void processMeshJob(MeshChunkJob* job);
    void processSaveJob(SaveChunkJob* job);
};

