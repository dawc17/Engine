#pragma once
#include "Chunk.h"
#include "Biome.h"
#include <cstdint>

uint32_t getWorldSeed();
void setWorldSeed(uint32_t seed);

void generateTerrain(BlockID* blocks, int cx, int cy, int cz);

BiomeID getBiomeAt(int worldX, int worldZ);

int getTerrainHeightAt(int worldX, int worldZ);

void getTerrainHeightsForChunk(int cx, int cz, int* outHeights);

