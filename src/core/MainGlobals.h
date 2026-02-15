#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include "../gameplay/Player.h"
#include "../world/ChunkManager.h"
#include "../world/WaterSimulator.h"
#include "../rendering/ParticleSystem.h"
#include "GameState.h"

class Shader;
class AudioEngine;

extern const int SCREEN_WIDTH;
extern const int SCREEN_HEIGHT;
extern const float MAX_RAYCAST_DISTANCE;

extern float fps;
extern float cameraSpeed;
extern int targetFps;

extern bool mouseLocked;
extern bool firstMouse;
extern double lastMouseX;
extern double lastMouseY;
extern bool showDebugMenu;
extern bool debugEnabled;

extern GameState currentState;
extern GameState settingsReturnState;
extern std::string currentWorldName;
extern float fov;
extern float mouseSensitivity;
extern bool wireframeMode;
extern bool showBiomeDebugColors;

extern bool drunkMode;
extern float drunkIntensity;
extern bool discoMode;
extern float discoSpeed;
extern bool earthquakeMode;
extern float earthquakeIntensity;

extern float worldTime;
extern float dayLength;
extern bool autoTimeProgression;
extern float fogDensity;
extern int renderDistance;

extern bool chatOpen;
extern bool chatFocusNext;
extern char chatInput[256];
extern std::vector<std::string> chatLog;

extern bool inventoryOpen;

extern bool enableCaustics;
extern bool isUnderwater;
extern const int SEA_LEVEL;

extern bool enableWaterSimulation;
extern int waterTickRate;
extern float waterTickAccumulator;
extern const float WATER_TICK_INTERVAL;
extern int frustumSolidTested;
extern int frustumSolidCulled;
extern int frustumSolidDrawn;
extern int frustumWaterTested;
extern int frustumWaterCulled;
extern int frustumWaterDrawn;

extern Player* g_player;
extern ChunkManager* g_chunkManager;
extern WaterSimulator* g_waterSimulator;
extern ParticleSystem* g_particleSystem;
extern AudioEngine* g_audioEngine;

extern std::unordered_map<uint8_t, GLuint> g_blockIcons;

void limitFPS(int targetFPS);
void framebuffer_size_callback(GLFWwindow* window, int width, int height);
void processInput(GLFWwindow* window, Player& player, float dt);
void mouseButtonCallback(GLFWwindow* window, int button, int action, int mods);
void scrollCallback(GLFWwindow* window, double xoffset, double yoffset);
GLuint loadHUDIcon(const unsigned char* pngData, unsigned int pngSize, bool useNearest = false);
void generateBlockIcons(GLuint textureArray, Shader* itemModelShader);
void unloadBlockIcons();
void executeCommand(const std::string& input, Player& player);
