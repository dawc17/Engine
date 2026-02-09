#include "SurvivalSystem.h"
#include "Player.h"
#include "../../libs/imgui/imgui.h"
#include <algorithm>
#include <cstdio>

static constexpr float MAX_HEALTH = 20.0f;
static constexpr float MAX_HUNGER = 20.0f;
static constexpr float HUNGER_DRAIN_PER_SEC = 0.01f;
static constexpr float HUNGER_REGEN_THRESHOLD = 18.0f;
static constexpr float HEALTH_REGEN_INTERVAL = 1.0f;
static constexpr float REGEN_HUNGER_COST = 0.2f;
static constexpr float HUNGER_DAMAGE_INTERVAL = 1.0f;
static constexpr float DROWN_INTERVAL = 1.0f;
static constexpr float FALL_DAMAGE_THRESHOLD = 3.0f;
static constexpr float FALL_DAMAGE_MULT = 2.0f;

static void applyDamage(Player& player, float amount)
{
    if (player.isDead)
        return;

    player.health -= amount;
    if (player.health <= 0.0f)
    {
        player.health = 0.0f;
        player.isDead = true;
    }
}

void initSurvival(Player& player)
{
    player.health = MAX_HEALTH;
    player.hunger = MAX_HUNGER;
    player.isDead = false;
    player.fallDistance = 0.0f;
    player.lastY = player.position.y;
    player.wasOnGround = player.onGround;
    player.hungerDamageTimer = 0.0f;
    player.regenTimer = 0.0f;
    player.drownTimer = 0.0f;
}

void updateSurvival(Player& player, float dt, bool isUnderwater)
{
    if (player.isDead)
        return;

    if (player.gamemode == Gamemode::Creative)
        return;

    player.hunger = std::clamp(player.hunger - dt * HUNGER_DRAIN_PER_SEC, 0.0f, MAX_HUNGER);

    if (player.onGround)
    {
        if (!player.wasOnGround)
        {
            float fall = player.fallDistance;
            if (fall > FALL_DAMAGE_THRESHOLD)
            {
                float dmg = (fall - FALL_DAMAGE_THRESHOLD) * FALL_DAMAGE_MULT;
                applyDamage(player, dmg);
            }
            player.fallDistance = 0.0f;
        }
    }
    else 
    {
        if (player.position.y < player.lastY)
            player.fallDistance += (player.lastY - player.position.y);
    }

    player.wasOnGround = player.onGround;
    player.lastY = player.position.y;

    if (player.hunger <= 0.0f)
    {
        player.hungerDamageTimer += dt;
        if (player.hungerDamageTimer >= HUNGER_DAMAGE_INTERVAL)
        {
            applyDamage(player, 1.0f);
            player.hungerDamageTimer = 0.0f;
        }
    }
    else 
    {
        player.hungerDamageTimer = 0.0f;
    }

    if (player.hunger >= HUNGER_REGEN_THRESHOLD && player.health < MAX_HEALTH)
    {
        player.regenTimer += dt;
        if (player.regenTimer >= HEALTH_REGEN_INTERVAL)
        {
            player.health = std::min(MAX_HEALTH, player.health + 1.0f);
            player.hunger = std::max(0.0f, player.hunger - REGEN_HUNGER_COST);
            player.regenTimer = 0.0f;
        }
    }
    else 
    {
        player.regenTimer = 0.0f;
    }

    if (isUnderwater)
    {
        player.drownTimer += dt;
        if (player.drownTimer >= DROWN_INTERVAL)
        {
            applyDamage(player, 1.0f);
            player.drownTimer = 0.0f;
        }
    }
    else 
    {
        player.drownTimer = 0.0f;
    }
}

void respawnPlayer(Player& player, const glm::vec3& pos)
{
    player.position = pos;
    player.velocity = glm::vec3(0.0f);
    player.health = MAX_HEALTH;
    player.hunger = MAX_HUNGER;
    player.isDead = false;
    player.fallDistance = 0.0f;
    player.lastY = player.position.y;
    player.wasOnGround = false;
    player.hungerDamageTimer = 0.0f;
    player.regenTimer = 0.0f;
    player.drownTimer = 0.0f;
}

void drawSurvivalHud(const Player& player, int fbWidth, int fbHeight)
{
    ImDrawList* drawList = ImGui::GetForegroundDrawList();

    float barWidth = 200.0f;
    float barHeight = 16.0f;
    float padding = 20.0f;
    float x = padding;
    float y = fbHeight - padding - barHeight * 2.0f - 8.0f;

    float healthFrac = player.health / MAX_HEALTH;
    if (healthFrac < 0.0f) healthFrac = 0.0f;
    if (healthFrac > 1.0f) healthFrac = 1.0f;

    float hungerFrac = player.hunger / MAX_HUNGER;
    if (hungerFrac < 0.0f) hungerFrac = 0.0f;
    if (hungerFrac > 1.0f) hungerFrac = 1.0f;

    ImU32 bg = IM_COL32(0, 0, 0, 150);
    ImU32 hp = IM_COL32(200, 40, 40, 220);
    ImU32 hg = IM_COL32(220, 170, 40, 220);
    ImU32 outline = IM_COL32(225, 225, 225, 80);

    ImVec2 hpMin(x, y);
    ImVec2 hpMax(x + barWidth, y + barHeight);
    drawList->AddRectFilled(hpMin, hpMax, bg, 4.0f);
    drawList->AddRectFilled(hpMin, ImVec2(x + barWidth * healthFrac, y + barHeight), hp, 4.0f);
    drawList->AddRect(hpMin, hpMax, outline, 4.0f, 0, 1.0f);

    ImVec2 hgMin(x, y + barHeight + 8.0f);
    ImVec2 hgMax(x + barWidth, y + barHeight + 8.0f + barHeight);
    drawList->AddRectFilled(hgMin, hgMax, bg, 4.0f);
    drawList->AddRectFilled(hgMin, ImVec2(x + barWidth * hungerFrac, hgMin.y + barHeight), hg, 4.0f);
    drawList->AddRect(hgMin, hgMax, outline, 4.0f, 0, 1.0f);

    char buf[32];
    std::snprintf(buf, sizeof(buf), "HP %.0f", player.health);
    drawList->AddText(ImVec2(x, y - 18.0f), IM_COL32(255, 255, 255, 220), buf);
    std::snprintf(buf, sizeof(buf), "H %.0f", player.hunger);
    drawList->AddText(ImVec2(x, hgMin.y - 18.0f), IM_COL32(255, 255, 255, 220), buf);

    if (player.isDead)
    {
        const char* msg = "You died - press R";
        ImVec2 textSize = ImGui::CalcTextSize(msg);
        ImVec2 pos ((fbWidth - textSize.x) * 0.5f, (fbHeight - textSize.y) * 0.5f);
        drawList->AddText(pos, IM_COL32(255, 80, 80, 255), msg);
    }
}