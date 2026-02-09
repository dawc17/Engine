#include "JobSystem.h"
#include "../world/ChunkManager.h"
#include "../world/TerrainGenerator.h"
#include "../world/CaveGenerator.h"
#include <cstring>
#include <algorithm>

JobSystem::JobSystem()
    : running(false), regionManager(nullptr), chunkManager(nullptr)
{
}

JobSystem::~JobSystem()
{
    stop();
}

void JobSystem::start(int numWorkers)
{
    if (running)
        return;

    running = true;

    for (int i = 0; i < numWorkers; i++)
    {
        workers.emplace_back(&JobSystem::workerLoop, this);
    }
}

void JobSystem::stop()
{
    if (!running)
        return;

    running = false;
    condition.notify_all();

    for (auto& worker : workers)
    {
        if (worker.joinable())
        {
            worker.join();
        }
    }

    workers.clear();
}

void JobSystem::enqueue(std::unique_ptr<Job> job)
{
    {
        std::lock_guard<std::mutex> lock(queueMutex);
        jobQueue.push(std::move(job));
    }
    condition.notify_one();
}

void JobSystem::enqueueHighPriority(std::unique_ptr<Job> job)
{
    {
        std::lock_guard<std::mutex> lock(queueMutex);
        highPriorityQueue.push(std::move(job));
    }
    condition.notify_one();
}

std::vector<std::unique_ptr<GenerateChunkJob>> JobSystem::pollCompletedGenerations()
{
    std::lock_guard<std::mutex> lock(completedMutex);
    std::vector<std::unique_ptr<GenerateChunkJob>> result;
    result.swap(completedGenerations);
    return result;
}

std::vector<std::unique_ptr<MeshChunkJob>> JobSystem::pollCompletedMeshes()
{
    std::lock_guard<std::mutex> lock(completedMutex);
    std::vector<std::unique_ptr<MeshChunkJob>> result;
    result.swap(completedMeshes);
    return result;
}

std::vector<std::unique_ptr<SaveChunkJob>> JobSystem::pollCompletedSaves()
{
    std::lock_guard<std::mutex> lock(completedMutex);
    std::vector<std::unique_ptr<SaveChunkJob>> result;
    result.swap(completedSaves);
    return result;
}

bool JobSystem::hasCompletedWork() const
{
    std::lock_guard<std::mutex> lock(const_cast<std::mutex&>(completedMutex));
    return !completedGenerations.empty() || !completedMeshes.empty() || !completedSaves.empty();
}

size_t JobSystem::pendingJobCount() const
{
    std::lock_guard<std::mutex> lock(const_cast<std::mutex&>(queueMutex));
    return jobQueue.size() + highPriorityQueue.size();
}

void JobSystem::workerLoop()
{
    while (running)
    {
        std::unique_ptr<Job> job;

        {
            std::unique_lock<std::mutex> lock(queueMutex);

            condition.wait(lock, [this] {
                return !running || !jobQueue.empty() || !highPriorityQueue.empty();
            });

            if (!running && jobQueue.empty() && highPriorityQueue.empty())
                break;

            if (!highPriorityQueue.empty())
            {
                job = std::move(highPriorityQueue.front());
                highPriorityQueue.pop();
            }
            else if (!jobQueue.empty())
            {
                job = std::move(jobQueue.front());
                jobQueue.pop();
            }
        }

        if (job)
        {
            processJob(std::move(job));
        }
    }
}

void JobSystem::processJob(std::unique_ptr<Job> job)
{
    switch (job->type)
    {
        case JobType::Generate:
            processGenerateJob(static_cast<GenerateChunkJob*>(job.get()));
            {
                std::lock_guard<std::mutex> lock(completedMutex);
                completedGenerations.push_back(
                    std::unique_ptr<GenerateChunkJob>(static_cast<GenerateChunkJob*>(job.release()))
                );
            }
            break;

        case JobType::Mesh:
            processMeshJob(static_cast<MeshChunkJob*>(job.get()));
            {
                std::lock_guard<std::mutex> lock(completedMutex);
                completedMeshes.push_back(
                    std::unique_ptr<MeshChunkJob>(static_cast<MeshChunkJob*>(job.release()))
                );
            }
            break;

        case JobType::Save:
            processSaveJob(static_cast<SaveChunkJob*>(job.get()));
            {
                std::lock_guard<std::mutex> lock(completedMutex);
                completedSaves.push_back(
                    std::unique_ptr<SaveChunkJob>(static_cast<SaveChunkJob*>(job.release()))
                );
            }
            break;
    }
}

void JobSystem::processGenerateJob(GenerateChunkJob* job)
{
  std::fill(std::begin(job->blocks), std::end(job->blocks), 0);
  std::fill(std::begin(job->skyLight), std::end(job->skyLight), MAX_SKY_LIGHT);

  if (regionManager && regionManager->loadChunkData(job->cx, job->cy, job->cz, job->blocks))
  {
    job->loadedFromDisk = true;
  }
  else
  {
    generateTerrain(job->blocks, job->cx, job->cy, job->cz);
    job->loadedFromDisk = false;

    // Compute terrain heights for this chunk's XZ columns
    int terrainHeights[CHUNK_SIZE * CHUNK_SIZE];
    getTerrainHeightsForChunk(job->cx, job->cz, terrainHeights);
    
    // Carve caves only on freshly generated chunks (not on loaded/saved ones)
    applyCavesToBlocks(job->blocks, glm::ivec3(job->cx, job->cy, job->cz), DEFAULT_WORLD_SEED, terrainHeights);
  }
}

void JobSystem::processMeshJob(MeshChunkJob* job)
{
    auto getBlock = [job](int x, int y, int z) -> BlockID
    {
        if (x >= 0 && x < CHUNK_SIZE &&
            y >= 0 && y < CHUNK_SIZE &&
            z >= 0 && z < CHUNK_SIZE)
        {
            return job->blocks[blockIndex(x, y, z)];
        }

        if (x >= CHUNK_SIZE && job->hasNeighborPosX)
        {
            int localY = y;
            int localZ = z;
            if (localY >= 0 && localY < CHUNK_SIZE && localZ >= 0 && localZ < CHUNK_SIZE)
                return job->neighborPosX[localY * CHUNK_SIZE + localZ];
        }
        if (x < 0 && job->hasNeighborNegX)
        {
            int localY = y;
            int localZ = z;
            if (localY >= 0 && localY < CHUNK_SIZE && localZ >= 0 && localZ < CHUNK_SIZE)
                return job->neighborNegX[localY * CHUNK_SIZE + localZ];
        }
        if (y >= CHUNK_SIZE && job->hasNeighborPosY)
        {
            int localX = x;
            int localZ = z;
            if (localX >= 0 && localX < CHUNK_SIZE && localZ >= 0 && localZ < CHUNK_SIZE)
                return job->neighborPosY[localX * CHUNK_SIZE + localZ];
        }
        if (y < 0 && job->hasNeighborNegY)
        {
            int localX = x;
            int localZ = z;
            if (localX >= 0 && localX < CHUNK_SIZE && localZ >= 0 && localZ < CHUNK_SIZE)
                return job->neighborNegY[localX * CHUNK_SIZE + localZ];
        }
        if (z >= CHUNK_SIZE && job->hasNeighborPosZ)
        {
            int localX = x;
            int localY = y;
            if (localX >= 0 && localX < CHUNK_SIZE && localY >= 0 && localY < CHUNK_SIZE)
                return job->neighborPosZ[localX * CHUNK_SIZE + localY];
        }
        if (z < 0 && job->hasNeighborNegZ)
        {
            int localX = x;
            int localY = y;
            if (localX >= 0 && localX < CHUNK_SIZE && localY >= 0 && localY < CHUNK_SIZE)
                return job->neighborNegZ[localX * CHUNK_SIZE + localY];
        }

        return 0;
    };

    auto getSkyLight = [job](int x, int y, int z) -> uint8_t
    {
        if (x >= 0 && x < CHUNK_SIZE &&
            y >= 0 && y < CHUNK_SIZE &&
            z >= 0 && z < CHUNK_SIZE)
        {
            return job->skyLight[blockIndex(x, y, z)];
        }

        if (x >= CHUNK_SIZE && job->hasNeighborPosX)
        {
            int localY = y;
            int localZ = z;
            if (localY >= 0 && localY < CHUNK_SIZE && localZ >= 0 && localZ < CHUNK_SIZE)
                return job->skyLightPosX[localY * CHUNK_SIZE + localZ];
        }
        if (x < 0 && job->hasNeighborNegX)
        {
            int localY = y;
            int localZ = z;
            if (localY >= 0 && localY < CHUNK_SIZE && localZ >= 0 && localZ < CHUNK_SIZE)
                return job->skyLightNegX[localY * CHUNK_SIZE + localZ];
        }
        if (y >= CHUNK_SIZE && job->hasNeighborPosY)
        {
            int localX = x;
            int localZ = z;
            if (localX >= 0 && localX < CHUNK_SIZE && localZ >= 0 && localZ < CHUNK_SIZE)
                return job->skyLightPosY[localX * CHUNK_SIZE + localZ];
        }
        if (y < 0 && job->hasNeighborNegY)
        {
            int localX = x;
            int localZ = z;
            if (localX >= 0 && localX < CHUNK_SIZE && localZ >= 0 && localZ < CHUNK_SIZE)
                return job->skyLightNegY[localX * CHUNK_SIZE + localZ];
        }
        if (z >= CHUNK_SIZE && job->hasNeighborPosZ)
        {
            int localX = x;
            int localY = y;
            if (localX >= 0 && localX < CHUNK_SIZE && localY >= 0 && localY < CHUNK_SIZE)
                return job->skyLightPosZ[localX * CHUNK_SIZE + localY];
        }
        if (z < 0 && job->hasNeighborNegZ)
        {
            int localX = x;
            int localY = y;
            if (localX >= 0 && localX < CHUNK_SIZE && localY >= 0 && localY < CHUNK_SIZE)
                return job->skyLightNegZ[localX * CHUNK_SIZE + localY];
        }

        return MAX_SKY_LIGHT;
    };

    buildChunkMeshOffThread(job->blocks, job->skyLight, getBlock, getSkyLight, 
                             job->vertices, job->indices,
                             job->waterVertices, job->waterIndices);
}

void JobSystem::processSaveJob(SaveChunkJob* job)
{
    if (regionManager)
    {
        regionManager->saveChunkData(job->cx, job->cy, job->cz, job->blocks);
    }
}
