#pragma once
#include <cstdint>

enum class BiomeID : uint8_t
{
    Desert = 0,
    Forest = 1,
    Tundra = 2,
    Plains = 3
};

enum class TreeType : uint8_t
{
    None = 0,
    Oak = 1,
    Spruce = 2
};

struct BiomeDefinition
{
    BiomeID id;
    uint8_t surfaceBlock;
    uint8_t fillerBlock;
    TreeType treeType;
    float terrainAmplitude;
    float treeDensity;
};

const BiomeDefinition& getBiomeDefinition(BiomeID biome);
BiomeID pickBiomeFromClimate(float temperature, float humidity);