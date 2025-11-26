#include "BlockTypes.h"

std::array<BlockType, 256> g_blockTypes;

void initBlockTypes()
{
    // Initialize all blocks as air (non-solid)
    for (auto &block : g_blockTypes)
    {
        block.solid = false;
        for (int i = 0; i < 6; i++)
        {
            block.faceTexture[i] = 0;
            block.faceRotation[i] = 0; // default: no rotation
        }
    }

    // Block 0 is air - leave as default

    // Block 1: Dirt (same texture on all sides)
    g_blockTypes[1].solid = true;
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
    for (int i = 0; i < 6; i++)
    {
        g_blockTypes[4].faceTexture[i] = 480; // sand (row 0, around column 18)
        g_blockTypes[4].faceRotation[i] = 0;
    }
    g_blockTypes[4].faceRotation[0] = 1; // +X: 180 deg
    g_blockTypes[4].faceRotation[1] = 1; // -X: 180 deg
    g_blockTypes[4].faceRotation[4] = 2; // +Z: 180 deg
    g_blockTypes[4].faceRotation[5] = 2; // -Z: 180 deg
}
