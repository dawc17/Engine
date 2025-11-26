#include "BlockTypes.h"

std::array<BlockType, 256> g_blockTypes;

void initBlockTypes()
{
    // Initialize all blocks as air (non-solid)
    for (auto &block : g_blockTypes)
    {
        block.solid = false;
        for (int i = 0; i < 6; i++)
            block.faceTexture[i] = 0;
    }

    // Block 0 is air - leave as default

    // Block 1: Dirt (same texture on all sides)
    g_blockTypes[1].solid = true;
    for (int i = 0; i < 6; i++)
        g_blockTypes[1].faceTexture[i] = 0; // tile 0 = dirt

    // Block 2: Grass (different top/side/bottom)
    g_blockTypes[2].solid = true;
    g_blockTypes[2].faceTexture[0] = 2; // +X side
    g_blockTypes[2].faceTexture[1] = 2; // -X side
    g_blockTypes[2].faceTexture[2] = 1; // +Y top (grass top)
    g_blockTypes[2].faceTexture[3] = 0; // -Y bottom (dirt)
    g_blockTypes[2].faceTexture[4] = 2; // +Z side
    g_blockTypes[2].faceTexture[5] = 2; // -Z side

    // Block 3: Stone
    g_blockTypes[3].solid = true;
    for (int i = 0; i < 6; i++)
        g_blockTypes[3].faceTexture[i] = 3; // tile 3 = stone

    // Block 4: Sand
    g_blockTypes[4].solid = true;
    for (int i = 0; i < 6; i++)
        g_blockTypes[4].faceTexture[i] = 8; // tile 4 = sand
}
