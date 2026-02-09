#pragma once
#include "Chunk.h"
#include "../utils/CoordUtils.h"
#include <queue>
#include <unordered_set>
#include <glm/glm.hpp>
#include <cmath>

struct ChunkManager;

constexpr uint8_t WATER_SOURCE = 9;
constexpr uint8_t WATER_FLOW_1 = 10;
constexpr uint8_t WATER_FLOW_2 = 11;
constexpr uint8_t WATER_FLOW_3 = 12;
constexpr uint8_t WATER_FLOW_4 = 13;
constexpr uint8_t WATER_FLOW_5 = 14;
constexpr uint8_t WATER_FLOW_6 = 15;
constexpr uint8_t WATER_FLOW_7 = 16;
constexpr uint8_t WATER_FALLING = 17;

constexpr int WATER_MAX_HORIZONTAL_SPREAD = 7;
constexpr int WATER_EDGE_SEARCH_DISTANCE = 4;
constexpr int WATER_TICK_RATE = 5;

inline bool isWater(uint8_t blockId)
{
    return blockId >= WATER_SOURCE && blockId <= WATER_FALLING;
}

inline bool isWaterSource(uint8_t blockId)
{
    return blockId == WATER_SOURCE;
}

inline bool isFallingWater(uint8_t blockId)
{
    return blockId == WATER_FALLING;
}

inline uint8_t getWaterLevel(uint8_t blockId)
{
    if (blockId == WATER_SOURCE) return 8;
    if (blockId == WATER_FALLING) return 8;
    if (blockId >= WATER_FLOW_1 && blockId <= WATER_FLOW_7)
        return 8 - (blockId - WATER_SOURCE);
    return 0;
}

inline int getRenderedDepth(uint8_t blockId)
{
    if (!isWater(blockId)) return -1;
    if (blockId == WATER_SOURCE) return 0;
    if (blockId == WATER_FALLING) return 8;
    return blockId - WATER_SOURCE;
}

inline float getLiquidHeightPercent(int depth)
{
    if (depth >= 8) depth = 0;
    return static_cast<float>(depth + 1) / 9.0f;
}

inline float getWaterHeight(uint8_t blockId)
{
    if (blockId == WATER_SOURCE || blockId == WATER_FALLING) return 0.875f;
    if (blockId >= WATER_FLOW_1 && blockId <= WATER_FLOW_7)
    {
        int depth = blockId - WATER_SOURCE;
        return 1.0f - getLiquidHeightPercent(depth);
    }
    return 0.0f;
}

inline uint8_t waterBlockFromLevel(uint8_t level)
{
    if (level >= 8) return WATER_SOURCE;
    if (level >= 1 && level <= 7) return WATER_SOURCE + (8 - level);
    return 0;
}

struct WaterUpdate
{
    int x, y, z;
    int scheduledTick;
};

class WaterSimulator
{
public:
    WaterSimulator();
    
    void setChunkManager(ChunkManager* cm) { chunkManager = cm; }
    
    void scheduleUpdate(int x, int y, int z);
    void tick();
    void onBlockChanged(int x, int y, int z, uint8_t oldBlock, uint8_t newBlock);
    
    int getTickRate() const { return tickRate; }
    void setTickRate(int rate) { tickRate = rate; }
    
private:
    ChunkManager* chunkManager = nullptr;
    std::queue<WaterUpdate> pendingUpdates;
    std::unordered_set<glm::ivec3, IVec3Hash> scheduledPositions;
    int tickRate = WATER_TICK_RATE;
    int currentTick = 0;
    
    void processWaterBlock(int x, int y, int z);
    void trySpreadDown(int x, int y, int z, uint8_t currentBlock);
    void trySpreadHorizontal(int x, int y, int z, uint8_t currentBlock);
    bool tryCreateSource(int x, int y, int z);
    int findDistanceToHole(int x, int y, int z, int depth);
    void scheduleNeighborUpdates(int x, int y, int z);
    
    uint8_t getBlock(int x, int y, int z);
    void setBlock(int x, int y, int z, uint8_t block);
    bool canFlowInto(int x, int y, int z);
    bool isSolid(int x, int y, int z);
};
