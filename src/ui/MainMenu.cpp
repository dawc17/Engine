#include "MainMenu.h"
#include "../core/GameState.h"
#include "../core/MainGlobals.h"
#include "../../libs/imgui/imgui.h"

#include <filesystem>
#include <algorithm>
#include <vector>

static char newWorldName[64] = {};

static void centerText(const char* text)
{
    float windowWidth = ImGui::GetWindowSize().x;
    float textWidth = ImGui::CalcTextSize(text).x;
    ImGui::SetCursorPosX((windowWidth - textWidth) * 0.5f);
    ImGui::Text("%s", text);
}

static bool centerButton(const char* label, const ImVec2& size)
{
    float windowWidth = ImGui::GetWindowSize().x;
    ImGui::SetCursorPosX((windowWidth - size.x) * 0.5f);
    return ImGui::Button(label, size);
}


std::vector<std::string> listSaveWorlds()
{
    std::vector<std::string> worlds;
    namespace fs = std::filesystem;

    std::string savesDir = "saves";
    if (!fs::exists(savesDir) || !fs::is_directory(savesDir))
        return worlds;

    for (const auto& entry : fs::directory_iterator(savesDir))
    {
        if (entry.is_directory())
            worlds.push_back(entry.path().filename().string());
    }

    std::sort(worlds.begin(), worlds.end());
    return worlds;
}

MenuResult drawMainMenu(int fbWidth, int fbHeight)
{
    MenuResult result;
    result.nextState = GameState::MainMenu;
    result.shouldQuit = false;

    ImVec2 windowSize(499.0f, 350.0f);
    ImVec2 windowPos((fbWidth - windowSize.x) * 0.5f, (fbHeight - windowSize.y) * 0.5f);

    ImGui::SetNextWindowPos(windowPos, ImGuiCond_Always);
    ImGui::SetNextWindowSize(windowSize, ImGuiCond_Always);
    ImGui::Begin("##MainMenu", nullptr,
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar |
        ImGuiWindowFlags_NoBackground);

    ImGui::Dummy(ImVec2(0.0f, 20.0f));

    ImGui::SetWindowFontScale(3.0f);
    centerText("VoxelEngine");
    ImGui::SetWindowFontScale(1.0f);

    ImGui::Dummy(ImVec2(0.0f, 40.0f));

    ImVec2 buttonSize(200.0f, 45.0f);

    if (centerButton("Play", buttonSize))
        result.nextState = GameState::WorldSelect;

    ImGui::Dummy(ImVec2(0.0f, 8.0f));

    if (centerButton("Settings", buttonSize))
        result.nextState = GameState::Settings;

    ImGui::Dummy(ImVec2(0.0f, 8.0f));

    if (centerButton("Quit", buttonSize))
        result.shouldQuit = true;

    ImGui::End();
    return result;
}

MenuResult drawWorldSelect(int fbWidth, int fbHeight)
{
    MenuResult result;
    result.nextState = GameState::WorldSelect;
    result.shouldQuit = false;

    ImVec2 windowSize(500.0f, 450.0f);
    ImVec2 windowPos((fbWidth - windowSize.x) * 0.5f, (fbHeight - windowSize.y) * 0.5f);

    ImGui::SetNextWindowPos(windowPos, ImGuiCond_Always);
    ImGui::SetNextWindowSize(windowSize, ImGuiCond_Always);
    ImGui::Begin("##WorldSelect", nullptr,
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove);

    ImGui::SetWindowFontScale(2.0f);
    centerText("Select World");
    ImGui::SetWindowFontScale(1.0f);

    ImGui::Separator();

    auto worlds = listSaveWorlds();

    ImGui::BeginChild("WorldList", ImVec2(0.0f, 250.0f), true);
    for (const auto& world : worlds)
    {
        if (ImGui::Selectable(world.c_str(), false, 0, ImVec2(0.0f, 30.0f)))
        {
            result.nextState = GameState::Playing;
            result.selectedWorld = world;
        }
    }
    if (worlds.empty())
    {
        ImGui::TextDisabled("No worlds found. Create one below!");
    }
    ImGui::EndChild();

    ImGui::Separator();

    ImGui::Text("New world:");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(200.0f);
    bool enterPressed = ImGui::InputText("##NewWorldName", newWorldName, sizeof(newWorldName),
        ImGuiInputTextFlags_EnterReturnsTrue);
    ImGui::SameLine();
    if ((ImGui::Button("Create") || enterPressed) && newWorldName[0] != '\0')
    {
        result.nextState = GameState::Playing;
        result.selectedWorld = std::string(newWorldName);
        newWorldName[0] = '\0';
    }

    ImGui::Dummy(ImVec2(0.0f, 10.0f));

    if (ImGui::Button("Back"))
        result.nextState = GameState::MainMenu;

    ImGui::End();
    return result;
}

MenuResult drawPauseMenu(int fbWidth, int fbHeight)
{
    MenuResult result;
    result.nextState = GameState::Paused;
    result.shouldQuit = false;

    ImDrawList* bg = ImGui::GetBackgroundDrawList();
    bg->AddRectFilled(
        ImVec2(0.0f, 0.0f),
        ImVec2(static_cast<float>(fbWidth), static_cast<float>(fbHeight)),
        IM_COL32(0, 0, 0, 150));

    ImVec2 windowSize(350.0f, 300.0f);
    ImVec2 windowPos((fbWidth - windowSize.x) * 0.5f, (fbHeight - windowSize.y) * 0.5f);

    ImGui::SetNextWindowPos(windowPos, ImGuiCond_Always);
    ImGui::SetNextWindowSize(windowSize, ImGuiCond_Always);
    ImGui::Begin("##PauseMenu", nullptr,
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBackground);

    ImGui::Dummy(ImVec2(0.0f, 10.0f));

    ImGui::SetWindowFontScale(2.5f);
    centerText("Paused");
    ImGui::SetWindowFontScale(1.0f);

    ImGui::Dummy(ImVec2(0.0f, 30.0f));

    ImVec2 buttonSize(250.0f, 40.0f);

    if (centerButton("Resume", buttonSize))
        result.nextState = GameState::Playing;

    ImGui::Dummy(ImVec2(0.0f, 8.0f));

    if (centerButton("Settings", buttonSize))
        result.nextState = GameState::Settings;

    ImGui::Dummy(ImVec2(0.0f, 8.0f));

    if (centerButton("Save & Quit to Menu", buttonSize))
        result.nextState = GameState::MainMenu;

    ImGui::End();
    return result;
}

MenuResult drawSettings(int fbWidth, int fbHeight, GameState returnState)
{
    MenuResult result;
    result.nextState = GameState::Settings;
    result.shouldQuit = false;

    if (returnState == GameState::Paused)
    {
        ImDrawList* bg = ImGui::GetBackgroundDrawList();
        bg->AddRectFilled(
            ImVec2(0.0f, 0.0f),
            ImVec2(static_cast<float>(fbWidth), static_cast<float>(fbHeight)),
            IM_COL32(0, 0, 0, 150));
    }

    ImVec2 windowSize(500.0f, 500.0f);
    ImVec2 windowPos((fbWidth - windowSize.x) * 0.5f, (fbHeight - windowSize.y) * 0.5f);

    ImGui::SetNextWindowPos(windowPos, ImGuiCond_Always);
    ImGui::SetNextWindowSize(windowSize, ImGuiCond_Always);
    ImGui::Begin("##Settings", nullptr,
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove);

    ImGui::SetWindowFontScale(2.0f);
    centerText("Settings");
    ImGui::SetWindowFontScale(1.0f);

    ImGui::Separator();

    ImGui::Text("GRAPHICS");
    ImGui::SliderInt("Render Distance", &renderDistance, 2, 16);
    ImGui::SliderFloat("FOV", &fov, 50.0f, 120.0f, "%.0f");
    ImGui::InputInt("Max FPS", &targetFps);
    if (targetFps < 10) targetFps = 10;
    if (targetFps > 1000) targetFps = 1000;
    ImGui::Checkbox("Caustics", &enableCaustics);
    ImGui::Checkbox("Wireframe", &wireframeMode);

    ImGui::Separator();

    ImGui::Text("GAMEPLAY");
    ImGui::SliderFloat("Mouse Sensitivity", &mouseSensitivity, 0.01f, 1.0f, "%.2f");
    ImGui::Checkbox("Water Simulation", &enableWaterSimulation);
    ImGui::Checkbox("Auto Day/Night", &autoTimeProgression);
    if (autoTimeProgression)
        ImGui::SliderFloat("Day Length (s)", &dayLength, 60.0f, 1200.0f);
    ImGui::SliderFloat("Fog Density", &fogDensity, 0.001f, 0.05f, "%.4f");

    ImGui::Separator();
    ImGui::Dummy(ImVec2(0.0f, 10.0f));

    if (ImGui::Button("Back"))
        result.nextState = returnState;

    ImGui::End();
    return result;
}