#pragma once

#include <memory>
#include <optional>
#include <string>

#include "../gameplay/Raycast.h"

struct GLFWwindow;
class RegionManager;
struct ChunkManager;
class JobSystem;
class WaterSimulator;
struct Player;

class WorldSession
{
public:
    std::unique_ptr<RegionManager> regionManager;
    std::unique_ptr<ChunkManager> chunkManager;
    std::unique_ptr<JobSystem> jobSystem;
    std::unique_ptr<WaterSimulator> waterSimulator;
    std::optional<RaycastHit> selectedBlock;

    bool active() const { return jobSystem != nullptr; }

    void init(const std::string& worldName, int requestedGamemode,
              Player& player, GLFWwindow* window, int numWorkers);
    void shutdown(Player& player, GLFWwindow* window);
};
