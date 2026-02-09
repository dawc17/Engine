#pragma once

#include "../gameplay/Player.h"

struct GLFWwindow;

void drawGameHUD(Player& player, int fbWidth, int fbHeight, bool inventoryOpen);
void drawChat(Player& player, GLFWwindow* window);
