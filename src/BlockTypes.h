#pragma once
#include <array>
#include <glm/glm.hpp>

// Atlas configuration
constexpr int ATLAS_TILES_X = 32;  // 512 / 16 = 32 tiles
constexpr int ATLAS_TILES_Y = 32;
constexpr float TILE_U = 1.0f / ATLAS_TILES_X;
constexpr float TILE_V = 1.0f / ATLAS_TILES_Y;

struct BlockType
{
    int faceTexture[6];  // tile index for +X -X +Y -Y +Z -Z
    int faceRotation[6]; // rotation for each face: 0=0°, 1=90°, 2=180°, 3=270° (CCW)
    bool solid;          // blocks movement/collision
    bool transparent;    // can see through (don't cull faces adjacent to this block)
};

extern std::array<BlockType, 256> g_blockTypes;
extern std::array<BlockType, 256> g_defaultBlockTypes;

void initBlockTypes();
void randomizeBlockTextures();
void resetBlockTextures();

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
