#include "DebugUI.h"
#include "../core/MainGlobals.h"
#include "../utils/BlockTypes.h"
#include "../gameplay/Inventory.h"
#include "../../libs/imgui/imgui.h"
#include <cmath>

ToolTransform g_toolTransform;

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

            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("Settings"))
        {
            ImGui::SliderInt("Render Distance", &renderDistance, 2, 16);

            ImGui::Separator();
            ImGui::Checkbox("Wireframe mode", &wireframeMode);
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

            ImGui::EndTabItem();
        }

        ImGui::EndTabBar();
    }

    ImGui::End();

    ImGui::Begin("Fun Bullshit", nullptr, debugFlags);

    if (ImGui::Button("Randomize Block Textures"))
    {
        randomizeBlockTextures();
        for (auto& pair : chunkManager->chunks)
            pair.second->dirtyMesh = true;
    }

    if (ImGui::Button("Reset Block Textures"))
    {
        resetBlockTextures();
        for (auto& pair : chunkManager->chunks)
            pair.second->dirtyMesh = true;
    }

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
