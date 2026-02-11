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
        18,
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

glm::vec3 getBiomeGrassTint(BiomeID biome)
{
    switch (biome)
    {
        case BiomeID::Desert: return glm::vec3(0.75f, 0.80f, 0.40f);
        case BiomeID::Forest: return glm::vec3(0.47f, 0.74f, 0.32f);
        case BiomeID::Tundra: return glm::vec3(0.55f, 0.70f, 0.55f);
        case BiomeID::Plains: return glm::vec3(0.58f, 0.82f, 0.38f);
        default: return glm::vec3(0.55f, 0.78f, 0.35f);
    }
}

glm::vec3 getBiomeFoliageTint(BiomeID biome)
{
    switch (biome)
    {
        case BiomeID::Desert: return glm::vec3(0.68f, 0.72f, 0.36f);
        case BiomeID::Forest: return glm::vec3(0.38f, 0.62f, 0.24f);
        case BiomeID::Tundra: return glm::vec3(0.42f, 0.56f, 0.42f);
        case BiomeID::Plains: return glm::vec3(0.48f, 0.72f, 0.30f);
        default: return glm::vec3(0.45f, 0.66f, 0.28f);
    }
}