#include "HUD.h"
#include "../core/MainGlobals.h"
#include "../gameplay/Inventory.h"
#include "../gameplay/SurvivalSystem.h"
#include "../../libs/imgui/imgui.h"
#include <GLFW/glfw3.h>
#include <string>

void drawGameHUD(Player& player, int fbWidth, int fbHeight, bool inventoryOpen)
{
    ImDrawList* drawList = ImGui::GetForegroundDrawList();

    if (player.gamemode == Gamemode::Survival)
        drawSurvivalHud(player, fbWidth, fbHeight);

    if (!inventoryOpen)
    {
        ImVec2 center(fbWidth * 0.5f, fbHeight * 0.5f);
        float crosshairSize = 10.0f;
        float crosshairThickness = 2.0f;
        ImU32 crosshairColor = IM_COL32(255, 255, 255, 200);

        drawList->AddLine(
            ImVec2(center.x - crosshairSize, center.y),
            ImVec2(center.x + crosshairSize, center.y),
            crosshairColor, crosshairThickness);
        drawList->AddLine(
            ImVec2(center.x, center.y - crosshairSize),
            ImVec2(center.x, center.y + crosshairSize),
            crosshairColor, crosshairThickness);
    }

    drawHotbar(player.inventory, fbWidth, fbHeight);
    if (inventoryOpen)
    {
        if (player.gamemode == Gamemode::Creative)
            drawCreativeInventoryScreen(player.inventory, fbWidth, fbHeight);
        else
            drawInventoryScreen(player.inventory, fbWidth, fbHeight);
    }
}

void drawChat(Player& player, GLFWwindow* window)
{
    if (!chatOpen || currentState != GameState::Playing)
        return;

    ImGui::SetNextWindowPos(ImVec2(20.0f, 20.0f), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(600.0f, 200.0f), ImGuiCond_Always);
    ImGui::Begin("Chat", nullptr, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize);

    ImGui::BeginChild("ChatLog", ImVec2(0.0f, -ImGui::GetFrameHeightWithSpacing()), false);
    for (const auto& line : chatLog)
        ImGui::TextUnformatted(line.c_str());
    ImGui::EndChild();

    if (chatFocusNext)
    {
        ImGui::SetKeyboardFocusHere();
        chatFocusNext = false;
    }

    if (ImGui::InputText("##ChatInput", chatInput, sizeof(chatInput), ImGuiInputTextFlags_EnterReturnsTrue))
    {
        std::string input(chatInput);
        chatInput[0] = 0;
        executeCommand(input, player);
        chatOpen = false;
        mouseLocked = true;
        glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
        firstMouse = true;
    }

    ImGui::End();
}
