#pragma once
#include "Chunk.h"
#include <cstdint>

uint32_t getWorldSeed();
void setWorldSeed(uint32_t seed);

void generateTerrain(BlockID* blocks, int cx, int cy, int cz);

int getTerrainHeightAt(int worldX, int worldZ);

void getTerrainHeightsForChunk(int cx, int cz, int* outHeights);

