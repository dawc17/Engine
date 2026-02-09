#include "TerrainGenerator.h"
#include "../thirdparty/PerlinNoise.hpp"
#include <cmath>
#include <cstdint>

static const siv::PerlinNoise::seed_type TERRAIN_SEED = 6767420;
static const siv::PerlinNoise perlin{TERRAIN_SEED};
static const siv::PerlinNoise perlinDetail{TERRAIN_SEED + 1};
static const siv::PerlinNoise perlinTrees{TERRAIN_SEED + 2};

constexpr int BASE_HEIGHT = 100;
constexpr int HEIGHT_VARIATION = 40;
constexpr int DIRT_DEPTH = 5;
constexpr int TREE_TRUNK_HEIGHT = 5;
constexpr int TREE_LEAF_RADIUS = 2;

constexpr uint8_t BLOCK_AIR = 0;
constexpr uint8_t BLOCK_DIRT = 1;
constexpr uint8_t BLOCK_GRASS = 2;
constexpr uint8_t BLOCK_STONE = 3;
constexpr uint8_t BLOCK_SAND = 4;
constexpr uint8_t BLOCK_LOG = 5;
constexpr uint8_t BLOCK_LEAVES = 6;
constexpr uint8_t BLOCK_WATER = 9;

constexpr int SEA_LEVEL = 116;

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

static void setBlockIfInChunk(BlockID* blocks, int localX, int localY, int localZ, uint8_t blockId, bool overwriteSolid = false)
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

void generateTerrain(BlockID* blocks, int cx, int cy, int cz)
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

            int terrainHeight = static_cast<int>(std::round(getTerrainHeight(worldX, worldZ)));

            for (int y = 0; y < CHUNK_SIZE; y++)
            {
                int worldY = worldOffsetY + y;
                int i = blockIndex(x, y, z);

                if (worldY > terrainHeight)
                {
                    if (worldY <= SEA_LEVEL)
                    {
                        blocks[i] = BLOCK_WATER;
                    }
                    else
                    {
                        blocks[i] = BLOCK_AIR;
                    }
                }
                else if (worldY == terrainHeight)
                {
                    if (terrainHeight <= SEA_LEVEL + 2 && terrainHeight >= SEA_LEVEL - 3)
                    {
                        blocks[i] = BLOCK_SAND;
                    }
                    else if (terrainHeight > SEA_LEVEL)
                    {
                        blocks[i] = BLOCK_GRASS;
                    }
                    else
                    {
                        blocks[i] = BLOCK_SAND;
                    }
                }
                else if (worldY > terrainHeight - DIRT_DEPTH)
                {
                    if (terrainHeight <= SEA_LEVEL + 2)
                    {
                        blocks[i] = BLOCK_SAND;
                    }
                    else
                    {
                        blocks[i] = BLOCK_DIRT;
                    }
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

            if (!shouldPlaceTree(worldX, worldZ))
                continue;

            int terrainHeight = static_cast<int>(std::round(getTerrainHeight(
                static_cast<float>(worldX), static_cast<float>(worldZ))));

            if (terrainHeight <= SEA_LEVEL + 2)
                continue;

            int treeBaseY = terrainHeight + 1;

            for (int ty = 0; ty < TREE_TRUNK_HEIGHT; ty++)
            {
                int localX = x;
                int localY = treeBaseY + ty - worldOffsetY;
                int localZ = z;
                setBlockIfInChunk(blocks, localX, localY, localZ, BLOCK_LOG, true);
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
                        setBlockIfInChunk(blocks, localX, localY, localZ, BLOCK_LEAVES);
                    }
                }
            }
        }
    }
}

int getTerrainHeightAt(int worldX, int worldZ)
{
    return static_cast<int>(std::round(getTerrainHeight(
        static_cast<float>(worldX), static_cast<float>(worldZ))));
}

void getTerrainHeightsForChunk(int cx, int cz, int* outHeights)
{
    int baseX = cx * CHUNK_SIZE;
    int baseZ = cz * CHUNK_SIZE;
    
    for (int z = 0; z < CHUNK_SIZE; ++z)
    {
        for (int x = 0; x < CHUNK_SIZE; ++x)
        {
            outHeights[z * CHUNK_SIZE + x] = getTerrainHeightAt(baseX + x, baseZ + z);
        }
    }
}

uint32_t getWorldSeed()
{
    return static_cast<uint32_t>(TERRAIN_SEED);
}