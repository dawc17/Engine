#include "Biome.h"

namespace
{
    const BiomeDefinition DESERT{
        BiomeID::Desert,
        4,
        4,
        TreeType::None,
        0.80f,
        0.0f
    };

    const BiomeDefinition FOREST{
        BiomeID::Forest,
        2,
        1,
        TreeType::Oak,
        1.05f,
        1.60f
    };

    const BiomeDefinition TUNDRA{
        BiomeID::Tundra,
        2,
        1,
        TreeType::Spruce,
        0.95f,
        0.45f
    };

    const BiomeDefinition PLAINS{
        BiomeID::Plains,
        2,
        1,
        TreeType::Oak,
        1.00f,
        0.60f
    };
}

const BiomeDefinition& getBiomeDefinition(BiomeID biome)
{
    switch (biome)
    {
        case BiomeID::Desert: return DESERT;
        case BiomeID::Forest: return FOREST;
        case BiomeID::Tundra: return TUNDRA;
        case BiomeID::Plains: return PLAINS;
        default: return PLAINS;
    }
}

BiomeID pickBiomeFromClimate(float temperature, float humidity)
{
    if (temperature > 0.68f && humidity < 0.45f)
    {
        return BiomeID::Desert;
    }

    if (temperature < 0.35f)
    {
        return BiomeID::Tundra;
    }

    if (humidity > 0.60f)
    {
        return BiomeID::Forest;
    }

    return BiomeID::Plains;
}