#include "TerrainGenerator.h"
#include "Biome.h"
#include "../thirdparty/PerlinNoise.hpp"
#include <cmath>
#include <cstdint>

static siv::PerlinNoise::seed_type TERRAIN_SEED = 6767420;
static siv::PerlinNoise perlin{TERRAIN_SEED};
static siv::PerlinNoise perlinDetail{TERRAIN_SEED + 1};
static siv::PerlinNoise perlinBiomeTemp{TERRAIN_SEED + 3};
static siv::PerlinNoise perlinBiomeHumidity{TERRAIN_SEED + 4};

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
constexpr uint8_t BLOCK_OAK_LOG = 5;
constexpr uint8_t BLOCK_OAK_LEAVES = 6;
constexpr uint8_t BLOCK_WATER = 9;
constexpr uint8_t BLOCK_SNOW_GRASS = 18;
constexpr uint8_t BLOCK_SPRUCE_LOG = 19;
constexpr uint8_t BLOCK_SPRUCE_LEAVES = 21;

constexpr int SEA_LEVEL = 116;

constexpr int TREE_GRID_SIZE = 7;
constexpr int TREE_OFFSET_RANGE = 10;
constexpr float TREE_SPAWN_CHANCE = 0.2f;

static bool shouldPlaceTree(int worldX, int worldZ, float chance)
{
    if (chance <= 0.0f)
        return false;

    if (chance > 1.0f)
        chance = 1.0f;

    int cellX = worldX >= 0 ? worldX / TREE_GRID_SIZE : (worldX - TREE_GRID_SIZE + 1) / TREE_GRID_SIZE;
    int cellZ = worldZ >= 0 ? worldZ / TREE_GRID_SIZE : (worldZ - TREE_GRID_SIZE + 1) / TREE_GRID_SIZE;

    unsigned int seedHash = static_cast<unsigned int>(TERRAIN_SEED) * 83492791u;
    unsigned int cellHash = static_cast<unsigned int>(cellX * 73856093) ^
                            static_cast<unsigned int>(cellZ * 19349663) ^
                            seedHash;

    float spawnChance = (cellHash % 10000) / 10000.0f;
    if (spawnChance >= chance)
        return false;

    unsigned int offsetHash = cellHash * 31337u;
    int offsetX = static_cast<int>(offsetHash % TREE_OFFSET_RANGE);
    int offsetZ = static_cast<int>((offsetHash / TREE_OFFSET_RANGE) % TREE_OFFSET_RANGE);

    int treePosX = cellX * TREE_GRID_SIZE + offsetX;
    int treePosZ = cellZ * TREE_GRID_SIZE + offsetZ;

    return worldX == treePosX && worldZ == treePosZ;
}

static BiomeID sampleBiome(float worldX, float worldZ)
{
    double temperature = perlinBiomeTemp.octave2D_01(
        worldX * 0.0015,
        worldZ * 0.0015,
        3,
        0.5
    );

    double humidity = perlinBiomeHumidity.octave2D_01(
        worldX * 0.0015,
        worldZ * 0.0015,
        3,
        0.5
    );

    return pickBiomeFromClimate(
        static_cast<float>(temperature),
        static_cast<float>(humidity)
    );
}

static float sampleTerrainAmplitude(float worldX, float worldZ)
{
    constexpr float SAMPLE_OFFSET = 12.0f;
    BiomeID b0 = sampleBiome(worldX, worldZ);
    BiomeID b1 = sampleBiome(worldX + SAMPLE_OFFSET, worldZ);
    BiomeID b2 = sampleBiome(worldX - SAMPLE_OFFSET, worldZ);
    BiomeID b3 = sampleBiome(worldX, worldZ + SAMPLE_OFFSET);
    BiomeID b4 = sampleBiome(worldX, worldZ - SAMPLE_OFFSET);

    float a0 = getBiomeDefinition(b0).terrainAmplitude;
    float a1 = getBiomeDefinition(b1).terrainAmplitude;
    float a2 = getBiomeDefinition(b2).terrainAmplitude;
    float a3 = getBiomeDefinition(b3).terrainAmplitude;
    float a4 = getBiomeDefinition(b4).terrainAmplitude;

    return a0 * 0.5f + (a1 + a2 + a3 + a4) * 0.125f;
}

static double getTerrainHeight(float worldX, float worldZ, float terrainAmplitude)
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

    return BASE_HEIGHT + blendedNoise * (HEIGHT_VARIATION * terrainAmplitude);
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

            BiomeID biomeId = sampleBiome(worldX, worldZ);
            const BiomeDefinition& biome = getBiomeDefinition(biomeId);
            float terrainAmplitude = sampleTerrainAmplitude(worldX, worldZ);
            int terrainHeight = static_cast<int>(std::round(getTerrainHeight(worldX, worldZ, terrainAmplitude)));
            uint8_t surfaceBlock = biome.surfaceBlock;
            uint8_t fillerBlock = biome.fillerBlock;
            if (terrainHeight < SEA_LEVEL)
            {
                surfaceBlock = biome.fillerBlock;
            }

            int beachMaxHeight = biomeId == BiomeID::Plains ? SEA_LEVEL : SEA_LEVEL + 1;
            bool isBeachColumn = biomeId != BiomeID::Desert &&
                                 terrainHeight >= SEA_LEVEL - 1 &&
                                 terrainHeight <= beachMaxHeight;
            if (isBeachColumn)
            {
                surfaceBlock = BLOCK_SAND;
                fillerBlock = BLOCK_SAND;
            }

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
                    blocks[i] = surfaceBlock;
                }
                else if (worldY > terrainHeight - DIRT_DEPTH)
                {
                    blocks[i] = fillerBlock;
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

            BiomeID biomeId = sampleBiome(static_cast<float>(worldX), static_cast<float>(worldZ));
            const BiomeDefinition& biome = getBiomeDefinition(biomeId);

            if (biome.treeType == TreeType::None)
                continue;

            if (!shouldPlaceTree(worldX, worldZ, TREE_SPAWN_CHANCE * biome.treeDensity))
                continue;

            float terrainAmplitude = sampleTerrainAmplitude(
                static_cast<float>(worldX), static_cast<float>(worldZ));
            int terrainHeight = static_cast<int>(std::round(getTerrainHeight(
                static_cast<float>(worldX), static_cast<float>(worldZ), terrainAmplitude)));

            if (terrainHeight <= SEA_LEVEL + 2)
                continue;

            uint8_t logBlock = BLOCK_OAK_LOG;
            uint8_t leafBlock = BLOCK_OAK_LEAVES;
            if (biome.treeType == TreeType::Spruce)
            {
                logBlock = BLOCK_SPRUCE_LOG;
                leafBlock = BLOCK_SPRUCE_LEAVES;
            }

            int treeBaseY = terrainHeight + 1;
            int trunkHeight = TREE_TRUNK_HEIGHT;
            if (biome.treeType == TreeType::Spruce)
            {
                trunkHeight = TREE_TRUNK_HEIGHT + 1;
            }

            for (int ty = 0; ty < trunkHeight; ty++)
            {
                int localX = x;
                int localY = treeBaseY + ty - worldOffsetY;
                int localZ = z;
                setBlockIfInChunk(blocks, localX, localY, localZ, logBlock, true);
            }

            int leafCenterY = treeBaseY + trunkHeight - 1;
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
                        setBlockIfInChunk(blocks, localX, localY, localZ, leafBlock);
                    }
                }
            }
        }
    }
}

BiomeID getBiomeAt(int worldX, int worldZ)
{
    return sampleBiome(static_cast<float>(worldX), static_cast<float>(worldZ));
}

int getTerrainHeightAt(int worldX, int worldZ)
{
    float terrainAmplitude = sampleTerrainAmplitude(
        static_cast<float>(worldX), static_cast<float>(worldZ));

    return static_cast<int>(std::round(getTerrainHeight(
        static_cast<float>(worldX), static_cast<float>(worldZ), terrainAmplitude)));
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

void setWorldSeed(uint32_t seed)
{
    TERRAIN_SEED = seed;
    perlin.reseed(TERRAIN_SEED);
    perlinDetail.reseed(TERRAIN_SEED + 1);
    perlinBiomeTemp.reseed(TERRAIN_SEED + 3);
    perlinBiomeHumidity.reseed(TERRAIN_SEED + 4);
}