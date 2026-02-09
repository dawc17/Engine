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

extern ToolTransform g_toolTransform;

void drawDebugUI(
    Player& player,
    ChunkManager* chunkManager,
    JobSystem* jobSystem,
    WaterSimulator* waterSimulator,
    std::optional<RaycastHit>& selectedBlock,
    float rawSunBrightness,
    bool& useAsyncLoading);
