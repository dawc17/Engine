#include "WaterSimulator.h"
#include "ChunkManager.h"
#include "../gameplay/Raycast.h"
#include "../utils/BlockTypes.h"
#include <algorithm>
#include <array>
#include <climits>

static const int DX[4] = {1, -1, 0, 0};
static const int DZ[4] = {0, 0, 1, -1};

WaterSimulator::WaterSimulator()
{
}

uint8_t WaterSimulator::getBlock(int x, int y, int z)
{
    if (!chunkManager) return 0;
    return getBlockAtWorld(x, y, z, *chunkManager);
}

void WaterSimulator::setBlock(int x, int y, int z, uint8_t block)
{
    if (!chunkManager) return;
    setBlockAtWorld(x, y, z, block, *chunkManager);
}

bool WaterSimulator::canFlowInto(int x, int y, int z)
{
    uint8_t block = getBlock(x, y, z);
    return block == 0 || isWater(block);
}

bool WaterSimulator::isSolid(int x, int y, int z)
{
    uint8_t block = getBlock(x, y, z);
    return isBlockSolid(block) && !isWater(block);
}

void WaterSimulator::scheduleUpdate(int x, int y, int z)
{
    glm::ivec3 pos(x, y, z);
    if (scheduledPositions.find(pos) != scheduledPositions.end())
        return;
    
    scheduledPositions.insert(pos);
    pendingUpdates.push({x, y, z, currentTick + tickRate});
}

void WaterSimulator::tick()
{
    currentTick++;
    
    std::vector<WaterUpdate> toProcess;
    
    while (!pendingUpdates.empty())
    {
        WaterUpdate& update = pendingUpdates.front();
        if (update.scheduledTick <= currentTick)
        {
            toProcess.push_back(update);
            glm::ivec3 pos(update.x, update.y, update.z);
            scheduledPositions.erase(pos);
            pendingUpdates.pop();
        }
        else
        {
            break;
        }
    }
    
    for (const auto& update : toProcess)
    {
        processWaterBlock(update.x, update.y, update.z);
    }
}

void WaterSimulator::onBlockChanged(int x, int y, int z, uint8_t oldBlock, uint8_t newBlock)
{
    if (isWater(newBlock))
    {
        scheduleUpdate(x, y, z);
    }
    
    if (isWater(oldBlock) || newBlock == 0)
    {
        scheduleUpdate(x, y - 1, z);
        scheduleUpdate(x + 1, y, z);
        scheduleUpdate(x - 1, y, z);
        scheduleUpdate(x, y, z + 1);
        scheduleUpdate(x, y, z - 1);
        scheduleUpdate(x, y + 1, z);
    }
    
    if (newBlock == 0)
    {
        for (int i = 0; i < 4; i++)
        {
            uint8_t neighbor = getBlock(x + DX[i], y, z + DZ[i]);
            if (isWater(neighbor)) scheduleUpdate(x, y, z);
        }
        
        uint8_t above = getBlock(x, y + 1, z);
        if (isWater(above)) scheduleUpdate(x, y, z);
    }
}

void WaterSimulator::processWaterBlock(int x, int y, int z)
{
    if (!chunkManager) return;
    
    uint8_t block = getBlock(x, y, z);
    
    if (block == 0)
    {
        uint8_t above = getBlock(x, y + 1, z);
        if (isWater(above))
        {
            setBlock(x, y, z, WATER_FALLING);
            scheduleUpdate(x, y, z);
            return;
        }
        
        uint8_t maxNeighborLevel = 0;
        for (int i = 0; i < 4; i++)
        {
            uint8_t neighbor = getBlock(x + DX[i], y, z + DZ[i]);
            if (isWater(neighbor))
            {
                uint8_t level = getWaterLevel(neighbor);
                if (level > maxNeighborLevel)
                    maxNeighborLevel = level;
            }
        }
        
        if (maxNeighborLevel > 1)
        {
            setBlock(x, y, z, waterBlockFromLevel(maxNeighborLevel - 1));
            scheduleUpdate(x, y, z);
        }
        return;
    }
    
    if (!isWater(block))
        return;
    
    if (isWaterSource(block))
    {
        trySpreadDown(x, y, z, block);
        trySpreadHorizontal(x, y, z, block);
        return;
    }
    
    if (tryCreateSource(x, y, z))
        return;
    
    uint8_t above = getBlock(x, y + 1, z);
    bool hasWaterAbove = isWater(above);
    
    if (hasWaterAbove)
    {
        if (block != WATER_FALLING)
        {
            setBlock(x, y, z, WATER_FALLING);
        }
        trySpreadDown(x, y, z, WATER_FALLING);
        trySpreadHorizontal(x, y, z, WATER_FALLING);
        return;
    }
    
    uint8_t maxNeighborLevel = 0;
    for (int i = 0; i < 4; i++)
    {
        uint8_t neighbor = getBlock(x + DX[i], y, z + DZ[i]);
        if (isWater(neighbor))
        {
            uint8_t level = getWaterLevel(neighbor);
            if (level > maxNeighborLevel)
                maxNeighborLevel = level;
        }
    }
    
    uint8_t expectedLevel = 0;
    if (maxNeighborLevel > 1)
    {
        expectedLevel = maxNeighborLevel - 1;
    }
    
    uint8_t currentLevel = getWaterLevel(block);
    
    if (expectedLevel == 0)
    {
        setBlock(x, y, z, 0);
        scheduleNeighborUpdates(x, y, z);
        return;
    }
    
    if (expectedLevel != currentLevel)
    {
        setBlock(x, y, z, waterBlockFromLevel(expectedLevel));
        scheduleNeighborUpdates(x, y, z);
    }
    
    trySpreadDown(x, y, z, block);
    if (currentLevel > 1)
    {
        trySpreadHorizontal(x, y, z, block);
    }
}

void WaterSimulator::trySpreadDown(int x, int y, int z, uint8_t currentBlock)
{
    uint8_t below = getBlock(x, y - 1, z);
    
    if (canFlowInto(x, y - 1, z) && !isSolid(x, y - 1, z))
    {
        if (!isWater(below))
        {
            setBlock(x, y - 1, z, WATER_FALLING);
            scheduleUpdate(x, y - 1, z);
        }
        else if (below != WATER_FALLING && below != WATER_SOURCE)
        {
            setBlock(x, y - 1, z, WATER_FALLING);
            scheduleUpdate(x, y - 1, z);
        }
    }
}

void WaterSimulator::trySpreadHorizontal(int x, int y, int z, uint8_t currentBlock)
{
    uint8_t currentLevel = getWaterLevel(currentBlock);
    if (currentLevel <= 1 && !isWaterSource(currentBlock) && !isFallingWater(currentBlock))
        return;
    
    uint8_t flowLevel;
    if (isWaterSource(currentBlock) || isFallingWater(currentBlock))
        flowLevel = 7;
    else
        flowLevel = currentLevel - 1;
    
    if (flowLevel < 1)
        return;
    
    std::array<int, 4> distances;
    std::fill(distances.begin(), distances.end(), INT_MAX);
    
    for (int i = 0; i < 4; i++)
    {
        int nx = x + DX[i];
        int nz = z + DZ[i];
        
        if (!canFlowInto(nx, y, nz))
            continue;
        
        if (!isSolid(nx, y - 1, nz))
        {
            distances[i] = 0;
            continue;
        }
        
        distances[i] = findDistanceToHole(nx, y, nz, WATER_EDGE_SEARCH_DISTANCE);
    }
    
    int minDist = *std::min_element(distances.begin(), distances.end());
    
    for (int i = 0; i < 4; i++)
    {
        int nx = x + DX[i];
        int nz = z + DZ[i];
        
        if (!canFlowInto(nx, y, nz))
            continue;
        
        bool shouldFlow = (minDist == INT_MAX) || (distances[i] <= minDist);
        
        if (shouldFlow)
        {
            uint8_t neighborBlock = getBlock(nx, y, nz);
            uint8_t neighborLevel = getWaterLevel(neighborBlock);
            
            if (neighborLevel < flowLevel)
            {
                setBlock(nx, y, nz, waterBlockFromLevel(flowLevel));
                scheduleUpdate(nx, y, nz);
            }
        }
    }
}

bool WaterSimulator::tryCreateSource(int x, int y, int z)
{
    uint8_t below = getBlock(x, y - 1, z);
    bool hasSolidBelow = isSolid(x, y - 1, z) || isWaterSource(below);
    
    if (!hasSolidBelow)
        return false;
    
    int adjacentSourceCount = 0;
    for (int i = 0; i < 4; i++)
    {
        uint8_t neighbor = getBlock(x + DX[i], y, z + DZ[i]);
        if (isWaterSource(neighbor))
            adjacentSourceCount++;
    }
    
    if (adjacentSourceCount >= 2)
    {
        setBlock(x, y, z, WATER_SOURCE);
        scheduleNeighborUpdates(x, y, z);
        return true;
    }
    
    return false;
}

int WaterSimulator::findDistanceToHole(int x, int y, int z, int depth)
{
    if (depth <= 0)
        return INT_MAX;
    
    if (!isSolid(x, y - 1, z))
        return 0;
    
    int minDist = INT_MAX;
    
    for (int i = 0; i < 4; i++)
    {
        int nx = x + DX[i];
        int nz = z + DZ[i];
        
        if (!canFlowInto(nx, y, nz))
            continue;
        
        int dist = findDistanceToHole(nx, y, nz, depth - 1);
        if (dist != INT_MAX && dist + 1 < minDist)
            minDist = dist + 1;
    }
    
    return minDist;
}

void WaterSimulator::scheduleNeighborUpdates(int x, int y, int z)
{
    scheduleUpdate(x + 1, y, z);
    scheduleUpdate(x - 1, y, z);
    scheduleUpdate(x, y + 1, z);
    scheduleUpdate(x, y - 1, z);
    scheduleUpdate(x, y, z + 1);
    scheduleUpdate(x, y, z - 1);
}
