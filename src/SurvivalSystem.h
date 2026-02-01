#pragma once
#include <glm/glm.hpp>

struct Player;

void initSurvival(Player& player);
void updateSurvival(Player& player, float dt, bool isUnderwater);
void respawnPlayer(Player& player, const glm::vec3& pos);
void drawSurvivalHud(const Player& player, int fbWidth, int fbHeight);