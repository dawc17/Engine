#include "JobSystem.h"
#include "ChunkManager.h"
#include "BlockTypes.h"
#include "PerlinNoise.hpp"
#include <cmath>
#include <cstring>
#include <algorithm>

static const siv::PerlinNoise::seed_type TERRAIN_SEED = 69420;
static const siv::PerlinNoise perlinJob{TERRAIN_SEED};
static const siv::PerlinNoise perlinDetailJob{TERRAIN_SEED + 1};
static const siv::PerlinNoise perlinTreesJob{TERRAIN_SEED + 2};

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

static bool shouldPlaceTreeJob(int worldX, int worldZ)
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

static double getTerrainHeightJob(float worldX, float worldZ)
{
    double continentNoise = perlinJob.octave2D_01(
        worldX * 0.002,
        worldZ * 0.002,
        2,
        0.5
    );

    continentNoise = std::pow(continentNoise, 1.2);

    double hillNoise = perlinJob.octave2D_01(
        worldX * 0.01,
        worldZ * 0.01,
        4,
        0.45
    );

    double detailNoise = perlinDetailJob.octave2D_01(
        worldX * 0.05,
        worldZ * 0.05,
        2,
        0.5
    );

    double blendedNoise = continentNoise * 0.4 + hillNoise * 0.5 + detailNoise * 0.1;
    blendedNoise = blendedNoise * blendedNoise * (3.0 - 2.0 * blendedNoise);

    return BASE_HEIGHT + blendedNoise * HEIGHT_VARIATION;
}

static void setBlockIfInChunkJob(BlockID* blocks, int localX, int localY, int localZ, uint8_t blockId, bool overwriteSolid = false)
{
    if (localX < 0 || localX >= CHUNK_SIZE ||
        localY < 0 || localY >= CHUNK_SIZE ||
        localZ < 0 || localZ >= CHUNK_SIZE)
        return;

    int idx = blockIndex(localX, localY, localZ);
    if (overwriteSolid || blocks[idx] == BLOCK_AIR)
    {
        blocks[idx] = blockId;
    }
}

static void generateTerrainJob(BlockID* blocks, int cx, int cy, int cz)
{
    int worldOffsetX = cx * CHUNK_SIZE;
    int worldOffsetY = cy * CHUNK_SIZE;
    int worldOffsetZ = cz * CHUNK_SIZE;

    for (int x = 0; x < CHUNK_SIZE; x++)
    {
        for (int z = 0; z < CHUNK_SIZE; z++)
        {
            float worldX = static_cast<float>(worldOffsetX + x);
            float worldZ = static_cast<float>(worldOffsetZ + z);

            int terrainHeight = static_cast<int>(std::round(getTerrainHeightJob(worldX, worldZ)));

            for (int y = 0; y < CHUNK_SIZE; y++)
            {
                int worldY = worldOffsetY + y;
                int i = blockIndex(x, y, z);

                if (worldY > terrainHeight)
                {
                    blocks[i] = BLOCK_AIR;
                }
                else if (worldY == terrainHeight)
                {
                    blocks[i] = BLOCK_GRASS;
                }
                else if (worldY > terrainHeight - DIRT_DEPTH)
                {
                    blocks[i] = BLOCK_DIRT;
                }
                else
                {
                    blocks[i] = BLOCK_STONE;
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

            if (!shouldPlaceTreeJob(worldX, worldZ))
                continue;

            int terrainHeight = static_cast<int>(std::round(getTerrainHeightJob(
                static_cast<float>(worldX), static_cast<float>(worldZ))));

            int treeBaseY = terrainHeight + 1;

            for (int ty = 0; ty < TREE_TRUNK_HEIGHT; ty++)
            {
                int localX = x;
                int localY = treeBaseY + ty - worldOffsetY;
                int localZ = z;
                setBlockIfInChunkJob(blocks, localX, localY, localZ, BLOCK_LOG, true);
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
                        setBlockIfInChunkJob(blocks, localX, localY, localZ, BLOCK_LEAVES);
                    }
                }
            }
        }
    }
}

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
        generateTerrainJob(job->blocks, job->cx, job->cy, job->cz);
        job->loadedFromDisk = false;
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

    buildChunkMeshOffThread(job->blocks, getBlock, job->vertices, job->indices);
}

void JobSystem::processSaveJob(SaveChunkJob* job)
{
    if (regionManager)
    {
        regionManager->saveChunkData(job->cx, job->cy, job->cz, job->blocks);
    }
}

