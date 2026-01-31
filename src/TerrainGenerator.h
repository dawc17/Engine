#pragma once
#include "Chunk.h"
#include <cstdint>

uint32_t getWorldSeed();

void generateTerrain(BlockID* blocks, int cx, int cy, int cz);

// Get terrain height at world X,Z coordinate
int getTerrainHeightAt(int worldX, int worldZ);

// Fill terrain heights for a chunk (CHUNK_SIZE x CHUNK_SIZE array, indexed as [z * CHUNK_SIZE + x])
void getTerrainHeightsForChunk(int cx, int cz, int* outHeights);

