#include "DebugUI.h"
#include "../core/MainGlobals.h"
#include "../utils/BlockTypes.h"
#include "../gameplay/Inventory.h"
#include "../world/TerrainGenerator.h"
#include "../../libs/imgui/imgui.h"
#include <cmath>

ToolTransform g_toolTransform;
BlockTransform g_blockTransform;
BlockSwingTuning g_blockSwingTuning;
BlockPlaceTuning g_blockPlaceTuning;

void drawDebugUI(
    Player& player,
    ChunkManager* chunkManager,
    JobSystem* jobSystem,
    WaterSimulator* waterSimulator,
    std::optional<RaycastHit>& selectedBlock,
    float rawSunBrightness,
    bool& useAsyncLoading)
{
    if (!showDebugMenu || currentState != GameState::Playing)
        return;

    ImGuiWindowFlags debugFlags = 0;
    if (mouseLocked)
        debugFlags |= ImGuiWindowFlags_NoInputs;
    ImGui::Begin("Debug", nullptr, debugFlags);

    if (ImGui::BeginTabBar("DebugTabs"))
    {
        if (ImGui::BeginTabItem("Info"))
        {
            ImGui::Text("FPS: %.1f", fps);
            ImGui::Text("Position: (%.2f, %.2f, %.2f)",
                        player.position.x, player.position.y, player.position.z);
            ImGui::Text("Velocity: (%.2f, %.2f, %.2f)",
                        player.velocity.x, player.velocity.y, player.velocity.z);
            ImGui::Text("Yaw: %.1f, Pitch: %.1f", player.yaw, player.pitch);
            ImGui::Text("On Ground: %s", player.onGround ? "Yes" : "No");

            int chunkXDbg = static_cast<int>(floor(player.position.x / 16.0f));
            int chunkZDbg = static_cast<int>(floor(player.position.z / 16.0f));
            ImGui::Text("Chunk: (%d, %d)", chunkXDbg, chunkZDbg);

            int playerBlockX = static_cast<int>(std::floor(player.position.x));
            int playerBlockZ = static_cast<int>(std::floor(player.position.z));
            BiomeID biome = getBiomeAt(playerBlockX, playerBlockZ);
            const char* biomeName = "Unknown";
            switch (biome)
            {
                case BiomeID::Desert: biomeName = "Desert"; break;
                case BiomeID::Forest: biomeName = "Forest"; break;
                case BiomeID::Tundra: biomeName = "Tundra"; break;
                case BiomeID::Plains: biomeName = "Plains"; break;
                default: break;
            }
            ImGui::Text("Biome: %s", biomeName);

            if (selectedBlock.has_value())
            {
                ImGui::Separator();
                ImGui::Text("Selected Block: (%d, %d, %d)",
                    selectedBlock->blockPos.x,
                    selectedBlock->blockPos.y,
                    selectedBlock->blockPos.z);
                uint8_t blockId = getBlockAtWorld(
                    selectedBlock->blockPos.x,
                    selectedBlock->blockPos.y,
                    selectedBlock->blockPos.z,
                    *chunkManager);
                ImGui::Text("Block ID: %d", blockId);
                ImGui::Text("Distance: %.2f", selectedBlock->distance);
            }
            else
            {
                ImGui::Separator();
                ImGui::Text("No block selected");
            }

            ImGui::Separator();

            const char* blockNames[] = {"Air", "Dirt", "Grass", "Stone", "Sand", "Oak Log", "Oak Leaves", "Glass", "Oak Planks", "Water"};
            const ItemStack& heldBlock = player.inventory.selectedItem();
            if (!heldBlock.isEmpty() && heldBlock.blockId < 10)
                ImGui::Text("Holding: %s x%d", blockNames[heldBlock.blockId], heldBlock.count);
            else if (!heldBlock.isEmpty())
                ImGui::Text("Holding: Block %d x%d", heldBlock.blockId, heldBlock.count);
            else
                ImGui::Text("Holding: Empty");
            ImGui::Text("Slot: %d/9", player.inventory.selectedHotbar + 1);

            ImGui::Separator();
            ImGui::Text("LMB: Break block");
            ImGui::Text("RMB: Place block");
            ImGui::Text("Space: Jump");

            ImGui::Separator();
            ImGui::Text("Chunks loaded: %zu", chunkManager->chunks.size());
            ImGui::Text("Chunks loading: %zu", chunkManager->loadingChunks.size());
            ImGui::Text("Chunks meshing: %zu", chunkManager->meshingChunks.size());
            ImGui::Text("Jobs pending: %zu", jobSystem->pendingJobCount());
            ImGui::Text("Frustum solid  tested:%d  culled:%d  drawn:%d", frustumSolidTested, frustumSolidCulled, frustumSolidDrawn);
            ImGui::Text("Frustum water  tested:%d  culled:%d  drawn:%d", frustumWaterTested, frustumWaterCulled, frustumWaterDrawn);

            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("Settings"))
        {
            ImGui::SliderInt("Render Distance", &renderDistance, 2, 16);

            ImGui::Separator();
            ImGui::Checkbox("Wireframe mode", &wireframeMode);
            ImGui::Checkbox("Biome Debug Colors", &showBiomeDebugColors);
            ImGui::Checkbox("Noclip mode", &player.noclip);
            ImGui::Checkbox("Async Loading", &useAsyncLoading);
            ImGui::SliderFloat("Move Speed", &cameraSpeed, 0.0f, 60.0f);

            ImGui::Separator();
            ImGui::InputInt("Max FPS", &targetFps);
            if (targetFps < 10) targetFps = 10;
            if (targetFps > 1000) targetFps = 1000;

            ImGui::Separator();
            ImGui::Text("DAY/NIGHT CYCLE");
            ImGui::Checkbox("Auto Time", &autoTimeProgression);
            ImGui::SliderFloat("Time of Day", &worldTime, 0.0f, 1.0f, "%.2f");
            ImGui::SliderFloat("Day Length (s)", &dayLength, 60.0f, 1200.0f);
            ImGui::SliderFloat("Fog Density", &fogDensity, 0.001f, 0.05f, "%.4f");

            const char* timeNames[] = {"Midnight", "Dawn", "Morning", "Noon", "Afternoon", "Dusk", "Evening", "Night"};
            int timeIndex = static_cast<int>(worldTime * 8.0f) % 8;
            ImGui::Text("Current: %s (Sun: %.0f%%)", timeNames[timeIndex], rawSunBrightness * 100.0f);

            ImGui::Separator();
            ImGui::Text("WATER");
            ImGui::Checkbox("Caustics", &enableCaustics);
            ImGui::Checkbox("Water Physics", &enableWaterSimulation);
            if (enableWaterSimulation)
            {
                ImGui::SliderInt("Water Tick Rate", &waterTickRate, 1, 20);
                waterSimulator->setTickRate(waterTickRate);
            }
            if (isUnderwater)
            {
                ImGui::TextColored(ImVec4(0.2f, 0.6f, 1.0f, 1.0f), "UNDERWATER");
            }
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("Tool Transform"))
        {
            ImGui::Text("POSITION");
            ImGui::SliderFloat("Pos X", &g_toolTransform.posX, -1.0f, 1.0f, "%.3f");
            ImGui::SliderFloat("Pos Y", &g_toolTransform.posY, -1.0f, 1.0f, "%.3f");
            ImGui::SliderFloat("Pos Z", &g_toolTransform.posZ, -2.0f, 0.0f, "%.3f");

            ImGui::Separator();
            ImGui::Text("ROTATION");
            ImGui::SliderFloat("Rot X", &g_toolTransform.rotX, -180.0f, 180.0f, "%.1f");
            ImGui::SliderFloat("Rot Y", &g_toolTransform.rotY, -180.0f, 180.0f, "%.1f");
            ImGui::SliderFloat("Rot Z", &g_toolTransform.rotZ, -180.0f, 180.0f, "%.1f");

            ImGui::Separator();
            ImGui::SliderFloat("Scale", &g_toolTransform.scale, 0.1f, 2.0f, "%.3f");

            ImGui::Separator();
            if (ImGui::Button("Reset"))
            {
                g_toolTransform = ToolTransform{};
            }
            ImGui::SameLine();
            ImGui::Text("  X:%.2f Y:%.2f Z:%.2f  rX:%.0f rY:%.0f rZ:%.0f  s:%.2f",
                g_toolTransform.posX, g_toolTransform.posY, g_toolTransform.posZ,
                g_toolTransform.rotX, g_toolTransform.rotY, g_toolTransform.rotZ, g_toolTransform.scale);

            ImGui::Separator();
            ImGui::Text("BLOCK POSITION");
            ImGui::SliderFloat("Block Pos X", &g_blockTransform.posX, -1.0f, 1.0f, "%.3f");
            ImGui::SliderFloat("Block Pos Y", &g_blockTransform.posY, -1.0f, 1.0f, "%.3f");
            ImGui::SliderFloat("Block Pos Z", &g_blockTransform.posZ, -2.0f, 0.0f, "%.3f");

            ImGui::Separator();
            ImGui::Text("BLOCK ROTATION");
            ImGui::SliderFloat("Block Rot X", &g_blockTransform.rotX, -180.0f, 180.0f, "%.1f");
            ImGui::SliderFloat("Block Rot Y", &g_blockTransform.rotY, -180.0f, 180.0f, "%.1f");
            ImGui::SliderFloat("Block Rot Z", &g_blockTransform.rotZ, -180.0f, 180.0f, "%.1f");

            ImGui::Separator();
            ImGui::SliderFloat("Block Scale", &g_blockTransform.scale, 0.05f, 2.0f, "%.3f");

            ImGui::Separator();
            if (ImGui::Button("Reset Block"))
            {
                g_blockTransform = BlockTransform{};
            }
            ImGui::SameLine();
            ImGui::Text("  X:%.2f Y:%.2f Z:%.2f  rX:%.0f rY:%.0f rZ:%.0f  s:%.2f",
                g_blockTransform.posX, g_blockTransform.posY, g_blockTransform.posZ,
                g_blockTransform.rotX, g_blockTransform.rotY, g_blockTransform.rotZ, g_blockTransform.scale);

            ImGui::Separator();
            ImGui::Text("BLOCK SWING ANIMATION");
            ImGui::SliderFloat("Block Swing Pos X", &g_blockSwingTuning.posX, -1.0f, 1.0f, "%.3f");
            ImGui::SliderFloat("Block Swing Pos Y", &g_blockSwingTuning.posY, -1.0f, 1.0f, "%.3f");
            ImGui::SliderFloat("Block Swing Pos Z", &g_blockSwingTuning.posZ, -1.0f, 1.0f, "%.3f");
            ImGui::SliderFloat("Block Swing Rot X", &g_blockSwingTuning.rotX, -180.0f, 180.0f, "%.1f");
            ImGui::SliderFloat("Block Swing Rot Z", &g_blockSwingTuning.rotZ, -180.0f, 180.0f, "%.1f");

            ImGui::Separator();
            if (ImGui::Button("Reset Block Swing"))
            {
                g_blockSwingTuning = BlockSwingTuning{};
            }
            ImGui::SameLine();
            ImGui::Text("  dX:%.2f dY:%.2f dZ:%.2f  dRX:%.0f dRZ:%.0f",
                g_blockSwingTuning.posX, g_blockSwingTuning.posY, g_blockSwingTuning.posZ,
                g_blockSwingTuning.rotX, g_blockSwingTuning.rotZ);

            ImGui::Separator();
            ImGui::Text("BLOCK PLACE ANIMATION");
            ImGui::SliderFloat("Block Place Pos X", &g_blockPlaceTuning.posX, -1.0f, 1.0f, "%.3f");
            ImGui::SliderFloat("Block Place Pos Y", &g_blockPlaceTuning.posY, -1.0f, 1.0f, "%.3f");
            ImGui::SliderFloat("Block Place Pos Z", &g_blockPlaceTuning.posZ, -1.0f, 1.0f, "%.3f");
            ImGui::SliderFloat("Block Place Rot X", &g_blockPlaceTuning.rotX, -180.0f, 180.0f, "%.1f");
            ImGui::SliderFloat("Block Place Rot Z", &g_blockPlaceTuning.rotZ, -180.0f, 180.0f, "%.1f");

            ImGui::Separator();
            if (ImGui::Button("Reset Block Place"))
            {
                g_blockPlaceTuning = BlockPlaceTuning{};
            }
            ImGui::SameLine();
            ImGui::Text("  dX:%.2f dY:%.2f dZ:%.2f  dRX:%.0f dRZ:%.0f",
                g_blockPlaceTuning.posX, g_blockPlaceTuning.posY, g_blockPlaceTuning.posZ,
                g_blockPlaceTuning.rotX, g_blockPlaceTuning.rotZ);

            ImGui::EndTabItem();
        }

        ImGui::EndTabBar();
    }

    ImGui::End();

    ImGui::Begin("Fun Bullshit", nullptr, debugFlags);

    ImGui::Separator();
    ImGui::Text("VISUAL CHAOS");

    ImGui::Checkbox("Drunk Mode", &drunkMode);
    if (drunkMode)
        ImGui::SliderFloat("Drunk Intensity", &drunkIntensity, 0.1f, 5.0f);

    ImGui::Checkbox("Disco Mode", &discoMode);
    if (discoMode)
        ImGui::SliderFloat("Disco Speed", &discoSpeed, 1.0f, 50.0f);

    ImGui::Checkbox("Earthquake", &earthquakeMode);
    if (earthquakeMode)
        ImGui::SliderFloat("Quake Intensity", &earthquakeIntensity, 0.05f, 2.0f);

    ImGui::End();
}
