#pragma once

#include "../core/GameState.h"
#include <string>
#include <vector>

struct MenuResult
{
    GameState nextState;
    std::string selectedWorld;
    bool shouldQuit;
};

MenuResult drawMainMenu(int fbWidth, int fbHeight);
MenuResult drawWorldSelect(int fbWidth, int fbHeight);
MenuResult drawPauseMenu(int fbWidth, int fbHeight);
MenuResult drawSettings(int fbWidth, int fbHeight, GameState returnState);
std::vector<std::string> listSaveWorlds();