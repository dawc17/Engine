#include "BlockTypes.h"
#include <random>

std::array<BlockType, 256> g_blockTypes;
std::array<BlockType, 256> g_defaultBlockTypes;

void initBlockTypes()
{
    // Initialize all blocks as air (non-solid, transparent)
    for (auto &block : g_blockTypes)
    {
        block.solid = false;
        block.transparent = true;  // Air is transparent
        for (int i = 0; i < 6; i++)
        {
            block.faceTexture[i] = 0;
            block.faceRotation[i] = 0; // default: no rotation
        }
    }

    // Block 0 is air - leave as default

    // Block 1: Dirt (same texture on all sides)
    g_blockTypes[1].solid = true;
    g_blockTypes[1].transparent = false;
    for (int i = 0; i < 6; i++)
    {
        g_blockTypes[1].faceTexture[i] = 229; // tile 2 = dirt
        g_blockTypes[1].faceRotation[i] = 0;
    }
    g_blockTypes[1].faceRotation[0] = 1; // +X: 180 deg
    g_blockTypes[1].faceRotation[1] = 1; // -X: 180 deg
    g_blockTypes[1].faceRotation[4] = 2; // +Z: 180 deg
    g_blockTypes[1].faceRotation[5] = 2; // -Z: 180 deg

    // Block 2: Grass (different top/side/bottom)
    g_blockTypes[2].solid = true;
    g_blockTypes[2].transparent = false;
    g_blockTypes[2].faceTexture[0] = 78; // +X side (grass side)
    g_blockTypes[2].faceTexture[1] = 78; // -X side
    g_blockTypes[2].faceTexture[2] = 174; // +Y top (grass top)
    g_blockTypes[2].faceTexture[3] = 229; // -Y bottom (dirt)
    g_blockTypes[2].faceTexture[4] = 78; // +Z side
    g_blockTypes[2].faceTexture[5] = 78; // -Z side
    // Flip all side faces upside down (180 deg)
    g_blockTypes[2].faceRotation[0] = 1; // +X: 180 deg
    g_blockTypes[2].faceRotation[1] = 1; // -X: 180 deg
    g_blockTypes[2].faceRotation[4] = 2; // +Z: 180 deg
    g_blockTypes[2].faceRotation[5] = 2; // -Z: 180 deg
    
    // Block 3: Stone
    g_blockTypes[3].solid = true;
    g_blockTypes[3].transparent = false;
    for (int i = 0; i < 6; i++)
    {
        g_blockTypes[3].faceTexture[i] = 72; // tile 1 = stone
        g_blockTypes[3].faceRotation[i] = 0;
    }
    g_blockTypes[3].faceRotation[0] = 1; // +X: 180 deg
    g_blockTypes[3].faceRotation[1] = 1; // -X: 180 deg
    g_blockTypes[3].faceRotation[4] = 2; // +Z: 180 deg
    g_blockTypes[3].faceRotation[5] = 2; // -Z: 180 deg

    // Block 4: Sand
    g_blockTypes[4].solid = true;
    g_blockTypes[4].transparent = false;
    for (int i = 0; i < 6; i++)
    {
        g_blockTypes[4].faceTexture[i] = 480; // sand (row 0, around column 18)
        g_blockTypes[4].faceRotation[i] = 0;
    }
    g_blockTypes[4].faceRotation[0] = 1; // +X: 180 deg
    g_blockTypes[4].faceRotation[1] = 1; // -X: 180 deg
    g_blockTypes[4].faceRotation[4] = 2; // +Z: 180 deg
    g_blockTypes[4].faceRotation[5] = 2; // -Z: 180 deg

    // Block 5: Oak Log (different top/bottom vs sides)
    g_blockTypes[5].solid = true;
    g_blockTypes[5].transparent = false;
    g_blockTypes[5].faceTexture[0] = 330; // +X side (bark)
    g_blockTypes[5].faceTexture[1] = 330; // -X side (bark)
    g_blockTypes[5].faceTexture[2] = 329; // +Y top (log top)
    g_blockTypes[5].faceTexture[3] = 329; // -Y bottom (log top)
    g_blockTypes[5].faceTexture[4] = 330; // +Z side (bark)
    g_blockTypes[5].faceTexture[5] = 330; // -Z side (bark)
    g_blockTypes[5].faceRotation[0] = 1;
    g_blockTypes[5].faceRotation[1] = 1;
    g_blockTypes[5].faceRotation[4] = 2;
    g_blockTypes[5].faceRotation[5] = 2;

    // Block 6: Oak Leaves (transparent - don't cull faces)
    g_blockTypes[6].solid = true;
    g_blockTypes[6].transparent = true;
    for (int i = 0; i < 6; i++)
    {
        g_blockTypes[6].faceTexture[i] = 328;
        g_blockTypes[6].faceRotation[i] = 0;
    }

    // Block 7: Glass (transparent, solid)
    g_blockTypes[7].solid = true;
    g_blockTypes[7].transparent = true;
    for (int i = 0; i < 6; i++)
    {
        g_blockTypes[7].faceTexture[i] = 264;
        g_blockTypes[7].faceRotation[i] = 0;
    }

    // Block 8: Oak Planks
    g_blockTypes[8].solid = true;
    g_blockTypes[8].transparent = false;
    for (int i = 0; i < 6; i++)
    {
        g_blockTypes[8].faceTexture[i] = 266;
        g_blockTypes[8].faceRotation[i] = 0;
    }
    g_blockTypes[8].faceRotation[0] = 1;
    g_blockTypes[8].faceRotation[1] = 1;
    g_blockTypes[8].faceRotation[4] = 2;
    g_blockTypes[8].faceRotation[5] = 2;

    g_defaultBlockTypes = g_blockTypes;
}

void randomizeBlockTextures()
{
    static std::random_device rd;
    static std::mt19937 gen(rd());
    std::uniform_int_distribution<int> dist(0, ATLAS_TILES_X * ATLAS_TILES_Y - 1);

    for (int blockId = 1; blockId < 256; blockId++)
    {
        if (g_blockTypes[blockId].solid)
        {
            for (int face = 0; face < 6; face++)
            {
                g_blockTypes[blockId].faceTexture[face] = dist(gen);
            }
        }
    }
}

void resetBlockTextures()
{
    g_blockTypes = g_defaultBlockTypes;
}
