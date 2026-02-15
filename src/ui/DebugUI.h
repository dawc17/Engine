#pragma once

#include "../gameplay/Player.h"
#include "../gameplay/Raycast.h"
#include "../world/ChunkManager.h"
#include "../world/WaterSimulator.h"
#include "../utils/JobSystem.h"
#include <optional>

struct ToolTransform
{
    float posX = 0.46f, posY = -0.23f, posZ = -0.54f;
    float rotX = -76.0f, rotY = 148.0f, rotZ = -84.0f;
    float scale = 0.47f;
};

struct BlockTransform
{
    float posX = 0.51f, posY = -0.31f, posZ = -0.63f;
    float rotX = 2.1f, rotY = 25.1f, rotZ = 3.2f;
    float scale = 0.248f;
};

struct BlockSwingTuning
{
    float posX = 0.0f, posY = -0.087f, posZ = 0.0f;
    float rotX = -16.9f, rotZ = 0.0f;
};

struct BlockPlaceTuning
{
    float posX = 0.04f, posY = -0.015f, posZ = -0.122f;
    float rotX = -15.2f, rotZ = 0.0f;
};

extern ToolTransform g_toolTransform;
extern BlockTransform g_blockTransform;
extern BlockSwingTuning g_blockSwingTuning;
extern BlockPlaceTuning g_blockPlaceTuning;

void drawDebugUI(
    Player& player,
    ChunkManager* chunkManager,
    JobSystem* jobSystem,
    WaterSimulator* waterSimulator,
    std::optional<RaycastHit>& selectedBlock,
    float rawSunBrightness,
    bool& useAsyncLoading);
