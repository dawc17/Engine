#include "MainGlobals.h"

#include <chrono>
#include <filesystem>
#include <iostream>
#include <sstream>
#include <thread>

#include <glm/glm.hpp>

#include "../libs/imgui/imgui.h"
#include "BlockTypes.h"
#include "Camera.h"
#include "Raycast.h"
#include "ShaderClass.h"
#include "TerrainGenerator.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

using FrameClock = std::chrono::high_resolution_clock;
using FrameDuration = std::chrono::duration<double>;
static FrameClock::time_point lastFrameTime = FrameClock::now();

const int SCREEN_WIDTH = 1920;
const int SCREEN_HEIGHT = 1080;
const float MAX_RAYCAST_DISTANCE = 8.0f;

float fps = 0.0f;
float cameraSpeed = 5.5f;
int targetFps = 144;

bool mouseLocked = true;
bool firstMouse = true;
double lastMouseX = SCREEN_WIDTH / 2.0;
double lastMouseY = SCREEN_HEIGHT / 2.0;
bool showDebugMenu = true;

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

const std::vector<uint8_t> PLACEABLE_BLOCKS = {1, 2, 3, 4, 5, 6, 7, 8};
int selectedBlockIndex = 2;

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

GLuint loadHUDIcon(const std::string& path)
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
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
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
    pushChatLine("gamemode placeholder kurwa im lazy");
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
  else if (!chatOpen && glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
  {
    glfwSetWindowShouldClose(window, true);
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

  if (player.isDead)
    return;


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
  if (glfwGetKey(window, GLFW_KEY_TAB) == GLFW_PRESS)
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
    return;

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

    if (oldBlock != 0 && g_particleSystem)
    {
      int tileIndex = g_blockTypes[oldBlock].faceTexture[0];
      glm::vec3 blockCenter = glm::vec3(hit->blockPos) + glm::vec3(0.5f);
      g_particleSystem->spawnBlockBreakParticles(blockCenter, tileIndex, 15);
    }

    setBlockAtWorld(hit->blockPos.x, hit->blockPos.y, hit->blockPos.z, 0, *g_chunkManager);

    if (g_waterSimulator)
    {
      g_waterSimulator->onBlockChanged(hit->blockPos.x, hit->blockPos.y, hit->blockPos.z, oldBlock, 0);
    }
  }
  else if (button == GLFW_MOUSE_BUTTON_RIGHT)
  {
    glm::ivec3 placePos = hit->blockPos + hit->normal;

    AABB playerAABB = g_player->getAABB();
    AABB blockAABB = AABB::fromBlockPos(placePos.x, placePos.y, placePos.z);

    if (playerAABB.intersects(blockAABB))
      return;

    uint8_t oldBlock = getBlockAtWorld(placePos.x, placePos.y, placePos.z, *g_chunkManager);
    uint8_t blockToPlace = PLACEABLE_BLOCKS[selectedBlockIndex];
    setBlockAtWorld(placePos.x, placePos.y, placePos.z, blockToPlace, *g_chunkManager);

    if (g_waterSimulator)
    {
      g_waterSimulator->onBlockChanged(placePos.x, placePos.y, placePos.z, oldBlock, blockToPlace);
    }
  }
}

void scrollCallback(GLFWwindow* window, double xoffset, double yoffset)
{
  ImGuiIO& io = ImGui::GetIO();
  if (io.WantCaptureMouse)
    return;

  if (!mouseLocked)
    return;

  int numBlocks = static_cast<int>(PLACEABLE_BLOCKS.size());
  if (yoffset > 0)
  {
    selectedBlockIndex = (selectedBlockIndex + 1) % numBlocks;
  }
  else if (yoffset < 0)
  {
    selectedBlockIndex = (selectedBlockIndex - 1 + numBlocks) % numBlocks;
  }
}
