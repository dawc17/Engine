#include "WorldSession.h"
#include "MainGlobals.h"
#include "../world/RegionManager.h"
#include "../world/ChunkManager.h"
#include "../world/WaterSimulator.h"
#include "../world/TerrainGenerator.h"
#include "../utils/JobSystem.h"
#include "../gameplay/Player.h"
#include "../rendering/ToolModelGenerator.h"
#include <filesystem>
#include <fstream>
#include <random>
#include <GLFW/glfw3.h>

void WorldSession::init(const std::string& worldName, int requestedGamemode,
                        Player& player, GLFWwindow* window, int numWorkers)
{
    currentWorldName = worldName;
    std::string worldPath = "saves/" + worldName;
    std::filesystem::create_directories(worldPath);

    std::string seedPath = worldPath + "/seed.dat";
    {
        std::ifstream seedFile(seedPath, std::ios::binary);
        if (seedFile.is_open())
        {
            uint32_t savedSeed = 0;
            seedFile.read(reinterpret_cast<char*>(&savedSeed), sizeof(savedSeed));
            setWorldSeed(savedSeed);
        }
        else
        {
            std::random_device rd;
            uint32_t newSeed = rd();
            setWorldSeed(newSeed);
            std::ofstream out(seedPath, std::ios::binary);
            out.write(reinterpret_cast<const char*>(&newSeed), sizeof(newSeed));
        }
    }

    regionManager = std::make_unique<RegionManager>(worldPath);
    chunkManager = std::make_unique<ChunkManager>();
    chunkManager->setRegionManager(regionManager.get());

    jobSystem = std::make_unique<JobSystem>();
    jobSystem->setRegionManager(regionManager.get());
    jobSystem->setChunkManager(chunkManager.get());
    chunkManager->setJobSystem(jobSystem.get());
    jobSystem->start(numWorkers);

    waterSimulator = std::make_unique<WaterSimulator>();
    waterSimulator->setChunkManager(chunkManager.get());

    g_chunkManager = chunkManager.get();
    g_waterSimulator = waterSimulator.get();

    PlayerData savedPlayer;
    if (regionManager->loadPlayerData(savedPlayer))
    {
        player.position = glm::vec3(savedPlayer.x, savedPlayer.y, savedPlayer.z);
        player.yaw = savedPlayer.yaw;
        player.pitch = savedPlayer.pitch;
        worldTime = savedPlayer.timeOfDay;
        player.health = savedPlayer.health;
        player.hunger = savedPlayer.hunger;
        player.gamemode = static_cast<Gamemode>(savedPlayer.gamemode);
        player.isDead = false;
    }
    else
    {
        player.position = glm::vec3(0.0f, 120.0f, 0.0f);
        player.velocity = glm::vec3(0.0f);
        player.yaw = 0.0f;
        player.pitch = 0.0f;
        player.health = 20.0f;
        player.hunger = 20.0f;
        player.gamemode = (requestedGamemode >= 0)
            ? static_cast<Gamemode>(requestedGamemode)
            : Gamemode::Survival;
        player.isDead = false;
        player.noclip = false;
    }

    std::string invPath = worldPath + "/inventory.dat";
    if (!player.inventory.loadFromFile(invPath))
    {
        for (auto& s : player.inventory.slots)
            s.clear();
        player.inventory.selectedHotbar = 0;
        player.inventory.heldItem.clear();

        if (player.gamemode == Gamemode::Creative)
        {
            const uint8_t defaultBlocks[] = {1, 2, 3, 4, 5, 6, 7, 8};
            for (int i = 0; i < 8; i++)
            {
                player.inventory.hotbar(i).blockId = defaultBlocks[i];
                player.inventory.hotbar(i).count = 64;
            }
            player.inventory.hotbar(9).blockId = TOOL_DIAMOND_PICKAXE;
            player.inventory.hotbar(9).count = 1;
        }
    }

    mouseLocked = true;
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
    firstMouse = true;
    currentState = GameState::Playing;
}

void WorldSession::shutdown(Player& player, GLFWwindow* window)
{
    if (!jobSystem)
        return;

    jobSystem->stop();

    PlayerData playerToSave;
    playerToSave.version = 2;
    playerToSave.x = player.position.x;
    playerToSave.y = player.position.y;
    playerToSave.z = player.position.z;
    playerToSave.yaw = player.yaw;
    playerToSave.pitch = player.pitch;
    playerToSave.timeOfDay = worldTime;
    playerToSave.health = player.health;
    playerToSave.hunger = player.hunger;
    playerToSave.gamemode = static_cast<int32_t>(player.gamemode);
    regionManager->savePlayerData(playerToSave);

    for (auto& pair : chunkManager->chunks)
    {
        Chunk* chunk = pair.second.get();
        regionManager->saveChunkData(
            chunk->position.x, chunk->position.y, chunk->position.z, chunk->blocks);
    }
    regionManager->flush();

    player.inventory.heldItem.clear();
    player.inventory.saveToFile("saves/" + currentWorldName + "/inventory.dat");
    inventoryOpen = false;

    chunkManager->chunks.clear();

    g_chunkManager = nullptr;
    g_waterSimulator = nullptr;

    waterSimulator.reset();
    chunkManager.reset();
    jobSystem.reset();
    regionManager.reset();

    selectedBlock.reset();

    mouseLocked = false;
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
    currentWorldName.clear();
}
