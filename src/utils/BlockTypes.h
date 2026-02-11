#pragma once
#include <array>
#include <cstdint>
#include <glm/glm.hpp>

enum BlockTex : int
{
    TEX_DIRT = 0,
    TEX_GRASS_TOP,
    TEX_GRASS_SIDE,
    TEX_GRASS_SIDE_SNOWED,
    TEX_STONE,
    TEX_SAND,
    TEX_LOG_OAK,
    TEX_LOG_OAK_TOP,
    TEX_LEAVES_OAK,
    TEX_GLASS,
    TEX_PLANKS_OAK,
    TEX_COBBLESTONE,
    TEX_LOG_SPRUCE,
    TEX_LOG_SPRUCE_TOP,
    TEX_LEAVES_SPRUCE,
    TEX_PLANKS_SPRUCE,
    TEX_SNOW,
    TEX_COUNT
};

float getBlockHardness(uint8_t blockId);

struct BlockType
{
    int faceTexture[6];
    int faceRotation[6];
    bool faceTint[6];
    bool solid;
    bool transparent;
    bool connectsToSame;
    bool isLiquid;
};

extern std::array<BlockType, 256> g_blockTypes;
extern std::array<BlockType, 256> g_defaultBlockTypes;

void initBlockTypes();

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
