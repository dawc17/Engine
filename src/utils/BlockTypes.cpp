#include "BlockTypes.h"

std::array<BlockType, 256> g_blockTypes;
std::array<BlockType, 256> g_defaultBlockTypes;

static void setAllFaces(BlockType& b, int tex)
{
    for (int i = 0; i < 6; i++)
    {
        b.faceTexture[i] = tex;
        b.faceRotation[i] = 0;
        b.faceTint[i] = false;
    }
}

static void setSolidOpaque(BlockType& b)
{
    b.solid = true;
    b.transparent = false;
    b.connectsToSame = false;
    b.isLiquid = false;
}

static void setSideRotations(BlockType& b)
{
    b.faceRotation[0] = 1;
    b.faceRotation[1] = 1;
    b.faceRotation[4] = 2;
    b.faceRotation[5] = 2;
}

void initBlockTypes()
{
    for (auto &block : g_blockTypes)
    {
        block.solid = false;
        block.transparent = true;
        block.connectsToSame = false;
        block.isLiquid = false;
        for (int i = 0; i < 6; i++)
        {
            block.faceTexture[i] = 0;
            block.faceRotation[i] = 0;
            block.faceTint[i] = false;
        }
    }

    {
        BlockType& b = g_blockTypes[1];
        setSolidOpaque(b);
        setAllFaces(b, TEX_DIRT);
        setSideRotations(b);
    }

    {
        BlockType& b = g_blockTypes[2];
        setSolidOpaque(b);
        b.faceTexture[0] = TEX_GRASS_SIDE;
        b.faceTexture[1] = TEX_GRASS_SIDE;
        b.faceTexture[2] = TEX_GRASS_TOP;
        b.faceTexture[3] = TEX_DIRT;
        b.faceTexture[4] = TEX_GRASS_SIDE;
        b.faceTexture[5] = TEX_GRASS_SIDE;
        setSideRotations(b);
        b.faceTint[0] = true;
        b.faceTint[1] = true;
        b.faceTint[2] = true;
        b.faceTint[3] = false;
        b.faceTint[4] = true;
        b.faceTint[5] = true;
    }

    {
        BlockType& b = g_blockTypes[3];
        setSolidOpaque(b);
        setAllFaces(b, TEX_STONE);
        setSideRotations(b);
    }

    {
        BlockType& b = g_blockTypes[4];
        setSolidOpaque(b);
        setAllFaces(b, TEX_SAND);
        setSideRotations(b);
    }

    {
        BlockType& b = g_blockTypes[5];
        setSolidOpaque(b);
        setAllFaces(b, TEX_LOG_OAK);
        b.faceTexture[2] = TEX_LOG_OAK_TOP;
        b.faceTexture[3] = TEX_LOG_OAK_TOP;
        setSideRotations(b);
    }

    {
        BlockType& b = g_blockTypes[6];
        b.solid = true;
        b.transparent = true;
        b.connectsToSame = false;
        b.isLiquid = false;
        setAllFaces(b, TEX_LEAVES_OAK);
        for (int i = 0; i < 6; i++)
            b.faceTint[i] = true;
    }

    {
        BlockType& b = g_blockTypes[7];
        b.solid = true;
        b.transparent = true;
        b.connectsToSame = true;
        b.isLiquid = false;
        setAllFaces(b, TEX_GLASS);
    }

    {
        BlockType& b = g_blockTypes[8];
        setSolidOpaque(b);
        setAllFaces(b, TEX_PLANKS_OAK);
        setSideRotations(b);
    }

    {
        BlockType& b = g_blockTypes[9];
        b.solid = false;
        b.transparent = true;
        b.connectsToSame = true;
        b.isLiquid = true;
        setAllFaces(b, 0);
    }

    for (int level = 0; level < 8; level++)
    {
        int blockId = 10 + level;
        g_blockTypes[blockId].solid = false;
        g_blockTypes[blockId].transparent = true;
        g_blockTypes[blockId].connectsToSame = true;
        g_blockTypes[blockId].isLiquid = true;
        setAllFaces(g_blockTypes[blockId], 0);
    }

    {
        BlockType& b = g_blockTypes[18];
        setSolidOpaque(b);
        b.faceTexture[0] = TEX_GRASS_SIDE_SNOWED;
        b.faceTexture[1] = TEX_GRASS_SIDE_SNOWED;
        b.faceTexture[2] = TEX_SNOW;
        b.faceTexture[3] = TEX_DIRT;
        b.faceTexture[4] = TEX_GRASS_SIDE_SNOWED;
        b.faceTexture[5] = TEX_GRASS_SIDE_SNOWED;
        setSideRotations(b);
        for (int i = 0; i < 6; i++)
            b.faceTint[i] = false;
    }

    {
        BlockType& b = g_blockTypes[19];
        setSolidOpaque(b);
        setAllFaces(b, TEX_LOG_SPRUCE);
        b.faceTexture[2] = TEX_LOG_SPRUCE_TOP;
        b.faceTexture[3] = TEX_LOG_SPRUCE_TOP;
        setSideRotations(b);
    }

    {
        BlockType& b = g_blockTypes[20];
        setSolidOpaque(b);
        setAllFaces(b, TEX_PLANKS_SPRUCE);
        setSideRotations(b);
    }

    {
        BlockType& b = g_blockTypes[21];
        b.solid = true;
        b.transparent = true;
        b.connectsToSame = false;
        b.isLiquid = false;
        setAllFaces(b, TEX_LEAVES_SPRUCE);
        for (int i = 0; i < 6; i++)
            b.faceTint[i] = true;
    }

    {
        BlockType& b = g_blockTypes[22];
        setSolidOpaque(b);
        setAllFaces(b, TEX_COBBLESTONE);
        setSideRotations(b);
    }

    g_defaultBlockTypes = g_blockTypes;
}

float getBlockHardness(uint8_t blockId)
{
    switch (blockId)
    {
        case 1: return 1.0f;
        case 2: return 1.2f;
        case 3: return 1.5f;
        case 4: return 0.8f;
        case 5: return 1.0f;
        case 6: return 0.2f;
        case 7: return 0.6f;
        case 8: return 1.0f;
        case 18: return 1.2f;
        case 19: return 1.0f;
        case 20: return 1.0f;
        case 21: return 0.2f;
        case 22: return 2.0f;
        default: return 1.0f;
    }
}
