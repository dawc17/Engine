#include "MainGlobals.h"

#include <chrono>
#include <filesystem>
#include <iostream>
#include <sstream>
#include <thread>

#include <glm/glm.hpp>

#include "../../libs/imgui/imgui.h"
#include "../utils/BlockTypes.h"
#include "../rendering/Camera.h"
#include "../utils/CoordUtils.h"
#include "GameState.h"
#include "../gameplay/Player.h"
#include "../gameplay/Raycast.h"
#include "../rendering/opengl/ShaderClass.h"
#include "../world/TerrainGenerator.h"

#define STB_IMAGE_IMPLEMENTATION
#include "../thirdparty/stb_image.h"

using FrameClock = std::chrono::high_resolution_clock;
using FrameDuration = std::chrono::duration<double>;
static FrameClock::time_point lastFrameTime = FrameClock::now();

const int SCREEN_WIDTH = 1920;
const int SCREEN_HEIGHT = 1080;
const float MAX_RAYCAST_DISTANCE = 8.0f;

float fps = 0.0f;
float cameraSpeed = 5.5f;
int targetFps = 60;

bool mouseLocked = false;
bool firstMouse = true;
double lastMouseX = SCREEN_WIDTH / 2.0;
double lastMouseY = SCREEN_HEIGHT / 2.0;
bool showDebugMenu = false;
bool debugEnabled = false;

GameState currentState = GameState::MainMenu;
GameState settingsReturnState = GameState::MainMenu;
std::string currentWorldName;
float fov = 70.0f;
float mouseSensitivity = 0.1f;
bool wireframeMode = false;

bool drunkMode = false;
float drunkIntensity = 1.0f;
bool discoMode = false;
float discoSpeed = 10.0f;
bool earthquakeMode = false;
float earthquakeIntensity = 0.3f;

float worldTime = 0.5f;
float dayLength = 600.0f;
bool autoTimeProgression = true;
float fogDensity = 0.008f;
int renderDistance = 4;

bool chatOpen = false;
bool chatFocusNext = false;
char chatInput[256] = {};
std::vector<std::string> chatLog;

bool enableCaustics = true;
bool isUnderwater = false;
const int SEA_LEVEL = 116;

bool enableWaterSimulation = true;
int waterTickRate = 5;
float waterTickAccumulator = 0.0f;
const float WATER_TICK_INTERVAL = 0.05f;

Player* g_player = nullptr;
ChunkManager* g_chunkManager = nullptr;
WaterSimulator* g_waterSimulator = nullptr;
ParticleSystem* g_particleSystem = nullptr;

bool inventoryOpen = false;

std::unordered_map<uint8_t, GLuint> g_blockIcons;

void limitFPS(int targetFPS)
{
  using namespace std::chrono;

  auto frameDuration = duration<double>(1.0 / targetFPS);
  auto now = FrameClock::now();
  auto elapsed = now - lastFrameTime;

  if (elapsed < frameDuration)
  {
    auto remaining = frameDuration - elapsed;

    if (remaining > milliseconds(2))
    {
      std::this_thread::sleep_for(remaining - milliseconds(2));
    }

    while (FrameClock::now() - lastFrameTime < frameDuration)
    {
      std::this_thread::yield();
    }
  }

  lastFrameTime = FrameClock::now();
}

GLuint loadHUDIcon(const std::string& path, bool useNearest)
{
  int width, height, channels;
  unsigned char* data = stbi_load(path.c_str(), &width, &height, &channels, 4);
  if (!data)
  {
    std::cerr << "Failed to load HUD icon: " << path << std::endl;
    return 0;
  }

  GLuint texture;
  glGenTextures(1, &texture);
  glBindTexture(GL_TEXTURE_2D, texture);
  GLenum filter = useNearest ? GL_NEAREST : GL_LINEAR;
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, filter);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, filter);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);

  stbi_image_free(data);
  return texture;
}

void loadBlockIcons(const std::string& basePath)
{
  std::unordered_map<uint8_t, std::string> iconFiles = {
      {1, "natural_dirt.png"},
      {2, "natural_grass_block.png"},
      {3, "masonry_cobblestone.png"},
      {4, "natural_sand.png"},
      {5, "log_oak.png"},
      {6, "leaves_oak.png"},
      {7, "transparent_glass.png"},
      {8, "planks_oak.png"}
  };

  for (const auto& [blockId, filename] : iconFiles)
  {
    std::string fullPath = basePath + "/" + filename;
    GLuint tex = loadHUDIcon(fullPath);
    if (tex != 0)
    {
      g_blockIcons[blockId] = tex;
      std::cout << "Loaded HUD icon for block " << (int)blockId << ": " << filename << std::endl;
    }
  }
}

void unloadBlockIcons()
{
  for (auto& [id, tex] : g_blockIcons)
  {
    glDeleteTextures(1, &tex);
  }
  g_blockIcons.clear();
}

static void pushChatLine(const std::string& line)
{
  chatLog.push_back(line);
  if (chatLog.size() > 200)
    chatLog.erase(chatLog.begin());
}

void executeCommand(const std::string& input, Player& player)
{
  if (input.empty())
    return;

  if (input[0] != '/')
  {
    pushChatLine(input);
    return;
  }

  std::istringstream iss(input);
  std::string cmd;
  iss >> cmd;

  if (cmd == "/noclip")
  {
    player.noclip = !player.noclip;
    pushChatLine(player.noclip ? "noclip on" : "noclip off");
    return;
  }

  if (cmd == "/time")
  {
    std::string arg;
    iss >> arg;
    if (arg == "set")
      iss >> arg;

    if (arg == "day") { worldTime = 0.25f; autoTimeProgression = false; pushChatLine("time set day"); return; }
    if (arg == "noon") { worldTime = 0.50f; autoTimeProgression = false; pushChatLine("time set noon"); return; }
    if (arg == "sunset") { worldTime = 0.75f; autoTimeProgression = false; pushChatLine("time set sunset"); return; }
    if (arg == "night") { worldTime = 0.90f; autoTimeProgression = false; pushChatLine("time set night"); return; }
    if (arg == "sunrise") { worldTime = 0.05f; autoTimeProgression = false; pushChatLine("time set sunrise"); return; }

    float t = 0.0f;
    std::istringstream argStream(arg);
    if (argStream >> t)
    {
      if (t < 0.0f) t = 0.0f;
      if (t > 1.0f) t = 1.0f;
      worldTime = t;
      autoTimeProgression = false;
      pushChatLine("time set " + std::to_string(t));
      return;
    }

    pushChatLine("usage: /time set <0..1> | day | noon | sunset | night | sunrise");
    return;
  }

  if (cmd == "/gamemode")
  {
    std::string arg;
    iss >> arg;

    if (arg == "creative" || arg == "1")
    {
      player.gamemode = Gamemode::Creative;
      player.health = 20.0f;
      player.hunger = 20.0f;
      player.isDead = false;

      bool hotbarEmpty = true;
      for (int i = 0; i < HOTBAR_SLOTS; i++)
      {
        if (!player.inventory.hotbar(i).isEmpty())
        {
          hotbarEmpty = false;
          break;
        }
      }
      if (hotbarEmpty)
      {
        const uint8_t defaultBlocks[] = {1, 2, 3, 4, 5, 6, 7, 8};
        for (int i = 0; i < 8; i++)
        {
          player.inventory.hotbar(i).blockId = defaultBlocks[i];
          player.inventory.hotbar(i).count = 64;
        }
      }

      pushChatLine("gamemode creative");
      return;
    }

    if (arg == "survival" || arg == "0")
    {
      player.gamemode = Gamemode::Survival;
      player.noclip = false;
      pushChatLine("gamemode survival");
      return;
    }

    pushChatLine("usage: /gamemode survival | creative | 0 | 1");
    return;
  }

  if (cmd == "/seed")
  {
    pushChatLine("seed " + std::to_string(getWorldSeed()));
    return;
  }

  pushChatLine("unknown command broski");
}

void framebuffer_size_callback(GLFWwindow* window, int width, int height)
{
  glViewport(0, 0, width, height);
  firstMouse = true;
}

void processInput(GLFWwindow* window, Player& player, float dt)
{
  if (chatOpen && glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
  {
    chatOpen = false;
    mouseLocked = true;
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
    firstMouse = true;
  }

  static bool tPressed = false;
  if (glfwGetKey(window, GLFW_KEY_T) == GLFW_PRESS)
  {
    if (!tPressed)
    {
      chatOpen = true;
      chatFocusNext = true;
      mouseLocked = false;
      glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
      tPressed = true;
    }
  }
  else
  {
    tPressed = false;
  }

  if (chatOpen)
    return;

  static bool ePressed = false;
  if (glfwGetKey(window, GLFW_KEY_E) == GLFW_PRESS)
  {
    if (!ePressed)
    {
      inventoryOpen = !inventoryOpen;
      if (inventoryOpen)
      {
        mouseLocked = false;
        glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
      }
      else
      {
        if (!player.inventory.heldItem.isEmpty())
        {
          player.inventory.addItem(player.inventory.heldItem.blockId, player.inventory.heldItem.count);
          player.inventory.heldItem.clear();
        }
        mouseLocked = true;
        glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
        firstMouse = true;
      }
      ePressed = true;
    }
  }
  else
  {
    ePressed = false;
  }

  if (inventoryOpen)
    return;

  if (player.isDead)
    return;

  for (int i = 0; i < 9; i++)
  {
    if (glfwGetKey(window, GLFW_KEY_1 + i) == GLFW_PRESS)
    {
      player.inventory.selectedHotbar = i;
      break;
    }
  }

  static bool uKeyPressed = false;
  if (glfwGetKey(window, GLFW_KEY_U) == GLFW_PRESS)
  {
    if (!uKeyPressed)
    {
      mouseLocked = !mouseLocked;
      if (mouseLocked)
      {
        glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
        firstMouse = true;
      }
      else
      {
        glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
      }
      uKeyPressed = true;
    }
  }
  else
  {
    uKeyPressed = false;
  }

  static bool tabPressed = false;
  if (debugEnabled && glfwGetKey(window, GLFW_KEY_TAB) == GLFW_PRESS)
  {
    if (!tabPressed)
    {
      showDebugMenu = !showDebugMenu;
      tabPressed = true;
    }
  }
  else
  {
    tabPressed = false;
  }

  glm::vec3 forward = playerForwardXZ(player);
  glm::vec3 right = playerRightXZ(player);
  glm::vec3 inputDir(0.0f);

  if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
    inputDir += forward;
  if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
    inputDir -= forward;
  if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
    inputDir -= right;
  if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
    inputDir += right;

  if (player.noclip)
  {
    if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS)
      inputDir.y += 1.0f;
    if (glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS)
      inputDir.y -= 1.0f;
  }

  player.applyMovement(inputDir, cameraSpeed);

  if (!player.noclip && glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS)
  {
    player.jump();
  }
}

std::string resolveTexturePath(const std::string& relativePath)
{
  namespace fs = std::filesystem;

  fs::path direct(relativePath);
  if (fs::exists(direct))
  {
    return direct.string();
  }

  fs::path exeDir = getExecutableDir();
  fs::path fromExe = exeDir / relativePath;
  if (fs::exists(fromExe))
  {
    return fromExe.string();
  }

  fs::path fromBuild = fs::path("..") / relativePath;
  if (fs::exists(fromBuild))
  {
    return fromBuild.string();
  }

  return relativePath;
}

void mouseButtonCallback(GLFWwindow* window, int button, int action, int mods)
{
  ImGuiIO& io = ImGui::GetIO();
  if (io.WantCaptureMouse)
    return;

  if (!mouseLocked)
    return;

  if (action != GLFW_PRESS)
  {
    if (button == GLFW_MOUSE_BUTTON_LEFT && g_player)
      g_player->isBreaking = false;
    return;
  }

  if (!g_player || !g_chunkManager)
    return;

  if (g_player->isDead)
    return;

  Camera tempCam{g_player->getEyePosition(), g_player->yaw, g_player->pitch, 70.0f};
  glm::vec3 forward = CameraForward(tempCam);
  glm::vec3 eyePos = g_player->getEyePosition();
  auto hit = raycastVoxel(eyePos, forward, MAX_RAYCAST_DISTANCE, *g_chunkManager);

  if (!hit.has_value())
    return;

  if (button == GLFW_MOUSE_BUTTON_LEFT)
  {
    uint8_t oldBlock = getBlockAtWorld(hit->blockPos.x, hit->blockPos.y, hit->blockPos.z, *g_chunkManager);
    if (oldBlock == 0)
      return;

    if (g_player->gamemode == Gamemode::Creative)
    {
      setBlockAtWorld(hit->blockPos.x, hit->blockPos.y, hit->blockPos.z, 0, *g_chunkManager);

      if (g_waterSimulator)
        g_waterSimulator->onBlockChanged(hit->blockPos.x, hit->blockPos.y, hit->blockPos.z, oldBlock, 0);

      if (g_particleSystem)
      {
        int tileIndex = g_blockTypes[oldBlock].faceTexture[0];
        glm::vec3 blockCenter = glm::vec3(hit->blockPos) + glm::vec3(0.5f);
        float skyLight = 1.0f;
        {
          glm::ivec3 cpos = worldToChunk(hit->blockPos.x, hit->blockPos.y, hit->blockPos.z);
          Chunk* c = g_chunkManager->getChunk(cpos.x, cpos.y, cpos.z);
          if (c)
          {
            glm::ivec3 local = worldToLocal(hit->blockPos.x, hit->blockPos.y, hit->blockPos.z);
            skyLight = static_cast<float>(c->skyLight[blockIndex(local.x, local.y, local.z)]) / static_cast<float>(MAX_SKY_LIGHT);
          }
        }
        g_particleSystem->spawnBlockBreakParticles(blockCenter, tileIndex, skyLight, 15);
      }

      return;
    }

    g_player->isBreaking = true;
    g_player->breakingBlockPos = hit->blockPos;
    g_player->breakingBlockId = oldBlock;
    g_player->breakProgress = 0.0f;
  }
  else if (button == GLFW_MOUSE_BUTTON_RIGHT)
  {
    const ItemStack& selected = g_player->inventory.selectedItem();
    if (selected.isEmpty())
      return;

    glm::ivec3 placePos = hit->blockPos + hit->normal;

    AABB playerAABB = g_player->getAABB();
    AABB blockAABB = AABB::fromBlockPos(placePos.x, placePos.y, placePos.z);

    if (playerAABB.intersects(blockAABB))
      return;

    uint8_t oldBlock = getBlockAtWorld(placePos.x, placePos.y, placePos.z, *g_chunkManager);
    uint8_t blockToPlace = selected.blockId;
    setBlockAtWorld(placePos.x, placePos.y, placePos.z, blockToPlace, *g_chunkManager);

    if (g_player->gamemode == Gamemode::Survival)
      g_player->inventory.removeFromSelected(1);

    if (g_waterSimulator)
      g_waterSimulator->onBlockChanged(placePos.x, placePos.y, placePos.z, oldBlock, blockToPlace);
  }
}

void scrollCallback(GLFWwindow* window, double xoffset, double yoffset)
{
    ImGuiIO& io = ImGui::GetIO();
    if (io.WantCaptureMouse)
        return;

    if (!mouseLocked)
        return;

    if (!g_player)
        return;

    if (yoffset > 0)
        g_player->inventory.selectedHotbar = (g_player->inventory.selectedHotbar + 1) % HOTBAR_SLOTS;
    else if (yoffset < 0)
        g_player->inventory.selectedHotbar = (g_player->inventory.selectedHotbar - 1 + HOTBAR_SLOTS) % HOTBAR_SLOTS;
}
