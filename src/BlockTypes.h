#pragma once
#include <array>
#include <glm/glm.hpp>

// Atlas configuration
constexpr int ATLAS_TILES_X = 16;
constexpr int ATLAS_TILES_Y = 16;
constexpr float TILE_U = 1.0f / ATLAS_TILES_X;
constexpr float TILE_V = 1.0f / ATLAS_TILES_Y;

struct BlockType
{
    int faceTexture[6]; // tile index for +X -X +Y -Y +Z -Z
    bool solid;
};

extern std::array<BlockType, 256> g_blockTypes;

void initBlockTypes();

// Convert tile index + local UV (0-1) to atlas UV
inline glm::vec2 atlasUV(int tileIndex, float localU, float localV)
{
    int tx = tileIndex % ATLAS_TILES_X;
    int ty = tileIndex / ATLAS_TILES_X;

    return glm::vec2(
        (tx + localU) * TILE_U,
        (ty + localV) * TILE_V
    );
}
