#pragma once
#include <cstdint>

enum class SurfaceType : uint8_t
{
    Default = 0,
    Grass,
    Stone,
    Wood,
    Gravel,
    Cloth,
    Ladder,
    Sand,
    Snow
};

inline SurfaceType blockToSurface(uint8_t blockId)
{
    switch (blockId) 
    {
        case 1: return SurfaceType::Gravel; // dirt
        case 2: return SurfaceType::Grass; // grass block
        case 3: return SurfaceType::Stone; // stone
        case 4: return SurfaceType::Sand; // sand
        case 5: return SurfaceType::Wood; // oak log
        case 6: return SurfaceType::Grass; // oak leaves
        case 7: return SurfaceType::Stone; // glass
        case 8: return SurfaceType::Wood; // oak planks
        case 18: return SurfaceType::Snow; // snowy grass
        case 19: return SurfaceType::Wood; // spruce log
        case 20: return SurfaceType::Wood; // spruce planks
        case 21: return SurfaceType::Grass; // spruce leaves
        case 22: return SurfaceType::Stone; // cobblestone
        default: return SurfaceType::Default;
    }
}