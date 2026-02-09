#pragma once
#include <array>
#include <cstdint>
#include <glm/glm.hpp>

// Atlas configuration
constexpr int ATLAS_TILES_X = 32;  // 512 / 16 = 32 tiles
constexpr int ATLAS_TILES_Y = 32;
constexpr float TILE_U = 1.0f / ATLAS_TILES_X;
constexpr float TILE_V = 1.0f / ATLAS_TILES_Y;
float getBlockHardness(uint8_t blockId);

struct BlockType
{
    int faceTexture[6];
    int faceRotation[6];
    bool solid;
    bool transparent;
    bool connectsToSame;
    bool isLiquid;
};

extern std::array<BlockType, 256> g_blockTypes;
extern std::array<BlockType, 256> g_defaultBlockTypes;

void initBlockTypes();
void randomizeBlockTextures();
void resetBlockTextures();

inline bool isBlockSolid(uint8_t blockId)
{
    if (blockId == 0) return false;
    return g_blockTypes[blockId].solid;
}

inline bool isBlockTransparent(uint8_t blockId)
{
    if (blockId == 0) return true;
    return g_blockTypes[blockId].transparent;
}

inline bool isBlockLiquid(uint8_t blockId)
{
    if (blockId == 0) return false;
    return g_blockTypes[blockId].isLiquid;
}

inline glm::vec2 atlasUV(int tileIndex, float localU, float localV)
{
    int tx = tileIndex % ATLAS_TILES_X;
    int ty = tileIndex / ATLAS_TILES_X;

    return glm::vec2(
        (tx + localU) * TILE_U,
        (ty + localV) * TILE_V
    );
}
