#ifdef _WIN32
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <mmsystem.h>
#pragma comment(lib, "winmm.lib")
#endif

#include "../libs/imgui/backends/imgui_impl_glfw.h"
#include "../libs/imgui/backends/imgui_impl_opengl3.h"
#include "../libs/imgui/imgui.h"
#include "ShaderClass.h"
#include "Camera.h"
#include "Chunk.h"
#include "ChunkManager.h"
#include "Meshing.h"
#include "BlockTypes.h"
#include "Raycast.h"
#include "Player.h"
#include "JobSystem.h"
#include "RegionManager.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <algorithm>
#include <cstdlib>
#include <exception>
#include <filesystem>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <iostream>
#include <string>
#include <vector>
#include <thread>
#include <chrono>
#include <unordered_map>

using FrameClock = std::chrono::high_resolution_clock;
using FrameDuration = std::chrono::duration<double>;
static FrameClock::time_point lastFrameTime = FrameClock::now();

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

void framebuffer_size_callback(GLFWwindow *window, int width, int height);
void processInput(GLFWwindow *window, Player &player, float dt);
void mouseButtonCallback(GLFWwindow *window, int button, int action, int mods);
void scrollCallback(GLFWwindow *window, double xoffset, double yoffset);
std::string resolveTexturePath(const std::string &relativePath);

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

bool enableCaustics = true;
bool isUnderwater = false;
const int SEA_LEVEL = 38;

// Global pointers for mouse callback
Player* g_player = nullptr;
ChunkManager* g_chunkManager = nullptr;

const std::vector<uint8_t> PLACEABLE_BLOCKS = {1, 2, 3, 4, 5, 6, 7, 8, 9};
int selectedBlockIndex = 2;  // Default to stone (index 2 = block ID 3)

// HUD block icons - maps block ID to OpenGL texture ID
std::unordered_map<uint8_t, GLuint> g_blockIcons;

// Load a single HUD icon texture
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

// Load all HUD block icons
void loadBlockIcons(const std::string& basePath)
{
    // Map block IDs to their icon filenames
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

// Cleanup HUD icons
void unloadBlockIcons()
{
    for (auto& [id, tex] : g_blockIcons)
    {
        glDeleteTextures(1, &tex);
    }
    g_blockIcons.clear();
}

int main()
{
  try
  {
#ifdef _WIN32
    timeBeginPeriod(1);
#endif

    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow *window = glfwCreateWindow(SCREEN_WIDTH, SCREEN_HEIGHT,
                                          "VoxelEngine", NULL, NULL);
    if (window == NULL)
    {
      std::cout << "Failed to initialize (bruh?)" << std::endl;
      glfwTerminate();
      return 1;
    }
    glfwMakeContextCurrent(window);
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

    gladLoadGL();
    glfwSwapInterval(0);

    glEnable(GL_DEPTH_TEST);

    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
    // Ensure a valid viewport before the first resize event fires.
    int fbWidth = 0, fbHeight = 0;
    glfwGetFramebufferSize(window, &fbWidth, &fbHeight);
    glViewport(0, 0, fbWidth, fbHeight);

    Shader shaderProgram("default.vert", "default.frag");
    shaderProgram.Activate();
    glUniform1i(glGetUniformLocation(shaderProgram.ID, "textureArray"), 0);
    GLint transformLoc = glGetUniformLocation(shaderProgram.ID, "transform");
    GLint modelLoc = glGetUniformLocation(shaderProgram.ID, "model");
    GLint timeOfDayLoc = glGetUniformLocation(shaderProgram.ID, "timeOfDay");
    GLint cameraPosLoc = glGetUniformLocation(shaderProgram.ID, "cameraPos");
    GLint skyColorLoc = glGetUniformLocation(shaderProgram.ID, "skyColor");
    GLint fogColorLoc = glGetUniformLocation(shaderProgram.ID, "fogColor");
    GLint fogDensityLoc = glGetUniformLocation(shaderProgram.ID, "fogDensity");
    GLint ambientLightLoc = glGetUniformLocation(shaderProgram.ID, "ambientLight");

    stbi_set_flip_vertically_on_load(false);

    // Load atlas and convert to 2D texture array (each tile becomes a layer)
    unsigned int textureArray;
    glGenTextures(1, &textureArray);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D_ARRAY, textureArray);
    
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    // Enable anisotropic filtering
    GLfloat maxAnisotropy = 0.0f;
    glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY, &maxAnisotropy);
    glTexParameterf(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAX_ANISOTROPY, maxAnisotropy);

    int width = 0, height = 0, nrChannels = 0;
    std::string texturePath =
        resolveTexturePath("assets/textures/blocks.png");
    unsigned char *data =
        stbi_load(texturePath.c_str(), &width, &height, &nrChannels, 0);

    const int TILE_SIZE = 16;  // Each tile is 16x16 pixels
    const int TILES_X = 32;     // 512 / 16 = 32 tiles wide
    const int TILES_Y = 32;     // 512 / 16 = 32 tiles tall
    const int NUM_TILES = TILES_X * TILES_Y;

    if (data)
    {
      GLenum internalFormat = (nrChannels == 4) ? GL_RGBA8 : GL_RGB8;
      GLenum format = (nrChannels == 4) ? GL_RGBA : GL_RGB;
      
      // Allocate storage for texture array
      glTexImage3D(GL_TEXTURE_2D_ARRAY, 0, internalFormat, 
                   TILE_SIZE, TILE_SIZE, NUM_TILES, 0, 
                   format, GL_UNSIGNED_BYTE, nullptr);
      
      // Copy each tile from the atlas into its own layer
      std::vector<unsigned char> tileData(TILE_SIZE * TILE_SIZE * nrChannels);
      int tileSizeBytes = TILE_SIZE * nrChannels;
      int atlasRowBytes = width * nrChannels;  // Full atlas row in bytes

      for (int ty = 0; ty < TILES_Y; ty++)
      {
        for (int tx = 0; tx < TILES_X; tx++)
        {
          int tileIndex = ty * TILES_X + tx;
          
          // Starting pixel of this tile in the atlas
          unsigned char *tileStart = data + (ty * TILE_SIZE) * atlasRowBytes + tx * tileSizeBytes;
          for (int row = 0; row < TILE_SIZE; row++)
          {
             std::copy(tileStart + row * atlasRowBytes, 
                       tileStart + row * atlasRowBytes + tileSizeBytes,
                       tileData.begin() + row * tileSizeBytes);
          }
          
          // Upload tile to its layer
          glTexSubImage3D(GL_TEXTURE_2D_ARRAY, 0, 
                          0, 0, tileIndex,
                          TILE_SIZE, TILE_SIZE, 1,
                          format, GL_UNSIGNED_BYTE, tileData.data());
        }
      }
      glGenerateMipmap(GL_TEXTURE_2D_ARRAY);
      std::cout << "Loaded texture array with " << NUM_TILES << " tiles" << std::endl;
    }
    else
    {
      std::cerr << "Failed to load texture at " << texturePath << ": "
                << stbi_failure_reason() << std::endl;
      // fallback magenta texture
      unsigned char fallback[] = {255, 0, 255, 255};
      glTexImage3D(GL_TEXTURE_2D_ARRAY, 0, GL_RGBA8, 1, 1, 1, 0, 
                   GL_RGBA, GL_UNSIGNED_BYTE, fallback);
    }
    stbi_image_free(data);

    // Initialize block type registry
    initBlockTypes();

    // Load HUD block icons
    std::string hudIconPath = resolveTexturePath("assets/textures/hud_blocks");
    loadBlockIcons(hudIconPath);

    Shader selectionShader("selection.vert", "selection.frag");
    GLint selectionTransformLoc = glGetUniformLocation(selectionShader.ID, "transform");
    GLint selectionColorLoc = glGetUniformLocation(selectionShader.ID, "color");

    Shader waterShader("water.vert", "water.frag");
    waterShader.Activate();
    glUniform1i(glGetUniformLocation(waterShader.ID, "textureArray"), 0);
    GLint waterTransformLoc = glGetUniformLocation(waterShader.ID, "transform");
    GLint waterModelLoc = glGetUniformLocation(waterShader.ID, "model");
    GLint waterTimeLoc = glGetUniformLocation(waterShader.ID, "time");
    GLint waterTimeOfDayLoc = glGetUniformLocation(waterShader.ID, "timeOfDay");
    GLint waterCameraPosLoc = glGetUniformLocation(waterShader.ID, "cameraPos");
    GLint waterSkyColorLoc = glGetUniformLocation(waterShader.ID, "skyColor");
    GLint waterFogColorLoc = glGetUniformLocation(waterShader.ID, "fogColor");
    GLint waterFogDensityLoc = glGetUniformLocation(waterShader.ID, "fogDensity");
    GLint waterAmbientLightLoc = glGetUniformLocation(waterShader.ID, "ambientLight");
    GLint waterEnableCausticsLoc = glGetUniformLocation(waterShader.ID, "enableCaustics");

    // Wireframe cube vertices (slightly larger than 1x1x1 to avoid z-fighting)
    const float s = 1.002f;  // Slight scale to avoid z-fighting
    const float o = -0.001f; // Offset
    float cubeVertices[] = {
        // Front face
        o, o, s+o,  s+o, o, s+o,  s+o, s+o, s+o,  o, s+o, s+o,
        // Back face  
        o, o, o,  s+o, o, o,  s+o, s+o, o,  o, s+o, o,
    };
    unsigned int cubeIndices[] = {
        // Front face lines
        0, 1, 1, 2, 2, 3, 3, 0,
        // Back face lines
        4, 5, 5, 6, 6, 7, 7, 4,
        // Connecting lines
        0, 4, 1, 5, 2, 6, 3, 7
    };

    GLuint selectionVAO, selectionVBO, selectionEBO;
    glGenVertexArrays(1, &selectionVAO);
    glGenBuffers(1, &selectionVBO);
    glGenBuffers(1, &selectionEBO);

    glBindVertexArray(selectionVAO);
    glBindBuffer(GL_ARRAY_BUFFER, selectionVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(cubeVertices), cubeVertices, GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, selectionEBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(cubeIndices), cubeIndices, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glBindVertexArray(0);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO &io = ImGui::GetIO();
    (void)io;
    ImGui::StyleColorsDark();
    
    // Set our mouse callback BEFORE ImGui init so ImGui can chain to it
    glfwSetMouseButtonCallback(window, mouseButtonCallback);
    glfwSetScrollCallback(window, scrollCallback);
    
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 460");

    bool wireframeMode = false;
    bool useAsyncLoading = true;

    Player player;
    float fov = 70.0f;

    float lastFrame = 0.0f;

    RegionManager regionManager("saves/world");

    PlayerData savedPlayer;
    if (regionManager.loadPlayerData(savedPlayer))
    {
      player.position = glm::vec3(savedPlayer.x, savedPlayer.y, savedPlayer.z);
      player.yaw = savedPlayer.yaw;
      player.pitch = savedPlayer.pitch;
      worldTime = savedPlayer.timeOfDay;
      std::cout << "Loaded player position: (" << savedPlayer.x << ", " << savedPlayer.y << ", " << savedPlayer.z << ")" << std::endl;
    }
    JobSystem jobSystem;
    jobSystem.setRegionManager(&regionManager);

    ChunkManager chunkManager;
    chunkManager.setRegionManager(&regionManager);
    chunkManager.setJobSystem(&jobSystem);
    jobSystem.setChunkManager(&chunkManager);

    int numWorkers = (std::max)(2, static_cast<int>(std::thread::hardware_concurrency()) - 1);
    jobSystem.start(numWorkers);
    std::cout << "Started job system with " << numWorkers << " worker threads" << std::endl;

    g_player = &player;
    g_chunkManager = &chunkManager;

    // Track selected block
    std::optional<RaycastHit> selectedBlock;

    // main draw loop sigma
    while (!glfwWindowShouldClose(window))
    {
      float currentFrame = glfwGetTime();
      float deltaTime = currentFrame - lastFrame;
      lastFrame = currentFrame;

      // Clamp deltaTime to avoid physics issues during lag spikes (e.g., window resize)
      // Max ~20 FPS equivalent to prevent falling through terrain
      const float MAX_DELTA_TIME = 0.05f;
      if (deltaTime > MAX_DELTA_TIME)
        deltaTime = MAX_DELTA_TIME;

      double mouseX, mouseY;
      glfwGetCursorPos(window, &mouseX, &mouseY);

      if (firstMouse)
      {
        lastMouseX = mouseX;
        lastMouseY = mouseY;
        firstMouse = false;
      }

      float xoffset = static_cast<float>(mouseX - lastMouseX);
      float yoffset = static_cast<float>(lastMouseY - mouseY);
      lastMouseX = mouseX;
      lastMouseY = mouseY;

      float sensitivity = 0.1f;
      xoffset *= sensitivity;
      yoffset *= sensitivity;

      if (mouseLocked)
      {
        player.yaw += xoffset;
        player.pitch += yoffset;
      }

      if (player.pitch > 89.0f)
        player.pitch = 89.0f;
      if (player.pitch < -89.0f)
        player.pitch = -89.0f;

      fps = 1.0f / deltaTime;

      processInput(window, player, deltaTime);
      player.update(deltaTime, chunkManager);

      if (autoTimeProgression)
      {
        worldTime += deltaTime / dayLength;
        if (worldTime > 1.0f) worldTime -= 1.0f;
      }

      float sunAngle = worldTime * 2.0f * 3.14159265f;
      float sunHeight = sin(sunAngle);
      float rawSunBrightness = glm::clamp(sunHeight * 2.0f + 0.3f, 0.0f, 1.0f);
      float moonLight = 0.25f;
      float sunBrightness = glm::max(rawSunBrightness, moonLight);
      float ambientLight = glm::mix(0.15f, 0.25f, rawSunBrightness);

      glm::vec3 dayColor(0.53f, 0.81f, 0.92f);
      glm::vec3 sunsetColor(1.0f, 0.55f, 0.3f);
      glm::vec3 nightColor(0.05f, 0.07f, 0.15f);

      glm::vec3 skyColor;
      if (sunHeight > 0.1f)
      {
        skyColor = dayColor;
      }
      else if (sunHeight > -0.1f)
      {
        float t = (sunHeight + 0.1f) / 0.2f;
        skyColor = glm::mix(sunsetColor, dayColor, t);
      }
      else
      {
        float t = glm::clamp((sunHeight + 0.1f) / 0.3f + 1.0f, 0.0f, 1.0f);
        skyColor = glm::mix(nightColor, sunsetColor, t);
      }

      glm::vec3 eyePos = player.getEyePosition();

      uint8_t blockAtEye = getBlockAtWorld(
          static_cast<int>(floor(eyePos.x)),
          static_cast<int>(floor(eyePos.y)),
          static_cast<int>(floor(eyePos.z)),
          chunkManager);
      isUnderwater = (blockAtEye == 9);

      glm::vec3 underwaterFogColor = glm::vec3(0.1f, 0.3f, 0.5f);
      float underwaterFogDensity = 0.12f;

      glm::vec3 fogCol = isUnderwater ? underwaterFogColor : glm::mix(skyColor * 0.8f, skyColor, 0.5f);
      glm::vec3 clearCol = isUnderwater ? underwaterFogColor : skyColor;
      float effectiveFogDensity = isUnderwater ? underwaterFogDensity : fogDensity;

      if (discoMode && !isUnderwater)
      {
        float t = static_cast<float>(glfwGetTime()) * discoSpeed;
        float r = (sin(t) + 1.0f) * 0.5f;
        float g = (sin(t * 1.3f + 2.094f) + 1.0f) * 0.5f;
        float b = (sin(t * 0.7f + 4.188f) + 1.0f) * 0.5f;
        glClearColor(r, g, b, 1.0f);
      }
      else
      {
        glClearColor(clearCol.r, clearCol.g, clearCol.b, 1.0f);
      }
      glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

      shaderProgram.Activate();
      glBindTexture(GL_TEXTURE_2D_ARRAY, textureArray);

      if (wireframeMode)
        glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
      else
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

      ImGui_ImplOpenGL3_NewFrame();
      ImGui_ImplGlfw_NewFrame();
      ImGui::NewFrame();

      glfwGetFramebufferSize(window, &fbWidth, &fbHeight);
      if (fbHeight == 0)
        fbHeight = 1;

      glm::mat4 model = glm::mat4(1.0f);

      float effectYaw = player.yaw;
      float effectPitch = player.pitch;

      if (drunkMode)
      {
        float t = static_cast<float>(glfwGetTime());
        effectYaw += sin(t * 2.0f) * 3.0f * drunkIntensity;
        effectPitch += sin(t * 1.7f) * 2.0f * drunkIntensity;
        effectYaw += sin(t * 0.5f) * 8.0f * drunkIntensity;
        eyePos.y += sin(t * 3.0f) * 0.1f * drunkIntensity;
      }

      if (earthquakeMode)
      {
        float t = static_cast<float>(glfwGetTime()) * 50.0f;
        eyePos.x += (sin(t * 1.1f) + sin(t * 2.3f) * 0.5f) * earthquakeIntensity;
        eyePos.y += (sin(t * 1.7f) + sin(t * 3.1f) * 0.5f) * earthquakeIntensity;
        eyePos.z += (sin(t * 1.3f) + sin(t * 2.7f) * 0.5f) * earthquakeIntensity;
      }

      Camera cam{eyePos, effectYaw, effectPitch, fov};
      glm::vec3 camForward = CameraForward(cam);
      glm::mat4 view = glm::lookAt(
          eyePos,
          eyePos + camForward,
          glm::vec3(0.0f, 1.0f, 0.0f));

      float aspect = static_cast<float>(fbWidth) / static_cast<float>(fbHeight);
      glm::mat4 proj = glm::perspective(glm::radians(fov), aspect, 0.1f, 1000.f);

      glm::mat4 mvp = proj * view * model;
      glUniformMatrix4fv(transformLoc, 1, GL_FALSE, glm::value_ptr(mvp));
      glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(model));
      glUniform1f(timeOfDayLoc, sunBrightness);
      glUniform3fv(cameraPosLoc, 1, glm::value_ptr(eyePos));
      glUniform3fv(skyColorLoc, 1, glm::value_ptr(clearCol));
      glUniform3fv(fogColorLoc, 1, glm::value_ptr(fogCol));
      glUniform1f(fogDensityLoc, effectiveFogDensity);
      glUniform1f(ambientLightLoc, ambientLight);

      int cx = floor(player.position.x / CHUNK_SIZE);
      int cz = floor(player.position.z / CHUNK_SIZE);

      const int LOAD_RADIUS = renderDistance;
      const int UNLOAD_RADIUS = LOAD_RADIUS + 2;
      const int CHUNK_HEIGHT_MIN = 0;
      const int CHUNK_HEIGHT_MAX = 4;

      chunkManager.update();

      for (int dx = -LOAD_RADIUS; dx <= LOAD_RADIUS; dx++)
      {
        for (int dz = -LOAD_RADIUS; dz <= LOAD_RADIUS; dz++)
        {
          for (int cy = CHUNK_HEIGHT_MIN; cy <= CHUNK_HEIGHT_MAX; cy++)
          {
            int chunkX = cx + dx;
            int chunkZ = cz + dz;

            if (!chunkManager.hasChunk(chunkX, cy, chunkZ) && 
                !chunkManager.isLoading(chunkX, cy, chunkZ) &&
                !chunkManager.isSaving(chunkX, cy, chunkZ))
            {
              if (useAsyncLoading)
              {
                chunkManager.enqueueLoadChunk(chunkX, cy, chunkZ);
              }
              else
              {
                chunkManager.loadChunk(chunkX, cy, chunkZ);
              }
            }
          }
        }
      }

      std::vector<ChunkManager::ChunkCoord> toUnload;
      for (auto &pair : chunkManager.chunks)
      {
        Chunk *chunk = pair.second.get();
        int distX = chunk->position.x - cx;
        int distZ = chunk->position.z - cz;
        if (std::abs(distX) > UNLOAD_RADIUS || std::abs(distZ) > UNLOAD_RADIUS)
        {
          if (!chunkManager.isSaving(chunk->position.x, chunk->position.y, chunk->position.z))
          {
            toUnload.push_back({chunk->position.x, chunk->position.y, chunk->position.z});
          }
        }
      }
      for (const auto &coord : toUnload)
      {
        if (useAsyncLoading)
        {
          chunkManager.enqueueSaveAndUnload(coord.x, coord.y, coord.z);
        }
        else
        {
          chunkManager.unloadChunk(coord.x, coord.y, coord.z);
        }
      }

      for (auto &pair : chunkManager.chunks)
      {
        Chunk *chunk = pair.second.get();
        if (chunk->dirtyMesh && !chunkManager.isMeshing(chunk->position.x, chunk->position.y, chunk->position.z))
        {
          bool neighborsReady = true;
          const int neighborOffsets[6][3] = {
            {1, 0, 0}, {-1, 0, 0},
            {0, 1, 0}, {0, -1, 0},
            {0, 0, 1}, {0, 0, -1}
          };
          for (const auto& offset : neighborOffsets)
          {
            int nx = chunk->position.x + offset[0];
            int ny = chunk->position.y + offset[1];
            int nz = chunk->position.z + offset[2];
            if (ny >= CHUNK_HEIGHT_MIN && ny <= CHUNK_HEIGHT_MAX)
            {
              if (!chunkManager.hasChunk(nx, ny, nz))
              {
                if (chunkManager.isLoading(nx, ny, nz))
                {
                  neighborsReady = false;
                  break;
                }
              }
            }
          }
          
          if (neighborsReady)
          {
            if (useAsyncLoading)
            {
              chunkManager.enqueueMeshChunk(chunk->position.x, chunk->position.y, chunk->position.z);
            }
            else
            {
              buildChunkMesh(*chunk, chunkManager);
              chunk->dirtyMesh = false;
            }
          }
        }

        if (chunk->indexCount > 0)
        {
          glm::mat4 chunkModel = glm::translate(glm::mat4(1.0f), glm::vec3(chunk->position.x * CHUNK_SIZE, chunk->position.y * CHUNK_SIZE, chunk->position.z * CHUNK_SIZE));
          glm::mat4 chunkMVP = proj * view * chunkModel;
          glUniformMatrix4fv(transformLoc, 1, GL_FALSE, glm::value_ptr(chunkMVP));
          glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(chunkModel));

          glBindVertexArray(chunk->vao);
          glDrawElements(GL_TRIANGLES, chunk->indexCount, GL_UNSIGNED_INT, 0);
        }
      }

      glEnable(GL_BLEND);
      glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
      glDepthMask(GL_FALSE);
      glEnable(GL_POLYGON_OFFSET_FILL);
      glPolygonOffset(1.0f, 1.0f);

      waterShader.Activate();

      float gameTime = static_cast<float>(glfwGetTime());
      glUniform1f(waterTimeLoc, gameTime);
      glUniform1f(waterTimeOfDayLoc, sunBrightness);
      glUniform3fv(waterCameraPosLoc, 1, glm::value_ptr(eyePos));
      glUniform3fv(waterSkyColorLoc, 1, glm::value_ptr(skyColor));

      glUniform3fv(waterFogColorLoc, 1, glm::value_ptr(fogCol));
      glUniform1f(waterFogDensityLoc, effectiveFogDensity);
      glUniform1f(waterAmbientLightLoc, ambientLight);
      glUniform1i(waterEnableCausticsLoc, enableCaustics ? 1 : 0);

      for (auto &pair : chunkManager.chunks)
      {
        Chunk *chunk = pair.second.get();
        if (chunk->waterIndexCount > 0)
        {
          glm::mat4 chunkModel = glm::translate(glm::mat4(1.0f), glm::vec3(chunk->position.x * CHUNK_SIZE, chunk->position.y * CHUNK_SIZE, chunk->position.z * CHUNK_SIZE));
          glm::mat4 chunkMVP = proj * view * chunkModel;
          glUniformMatrix4fv(waterTransformLoc, 1, GL_FALSE, glm::value_ptr(chunkMVP));
          glUniformMatrix4fv(waterModelLoc, 1, GL_FALSE, glm::value_ptr(chunkModel));

          glBindVertexArray(chunk->waterVao);
          glDrawElements(GL_TRIANGLES, chunk->waterIndexCount, GL_UNSIGNED_INT, 0);
        }
      }

      glDepthMask(GL_TRUE);
      glDisable(GL_BLEND);
      glDisable(GL_POLYGON_OFFSET_FILL);

      // Raycast for block selection
      selectedBlock = raycastVoxel(eyePos, camForward, MAX_RAYCAST_DISTANCE, chunkManager);

      // Render selection highlight
      if (selectedBlock.has_value())
      {
        glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
        glLineWidth(2.0f);
        // Use polygon offset to push lines slightly forward, avoiding z-fighting
        glEnable(GL_POLYGON_OFFSET_LINE);
        glPolygonOffset(-1.0f, -1.0f);

        selectionShader.Activate();
        glm::mat4 selectionModel = glm::translate(glm::mat4(1.0f), 
            glm::vec3(selectedBlock->blockPos));
        glm::mat4 selectionMVP = proj * view * selectionModel;
        glUniformMatrix4fv(selectionTransformLoc, 1, GL_FALSE, glm::value_ptr(selectionMVP));
        glUniform4f(selectionColorLoc, 0.0f, 0.0f, 0.0f, 1.0f);  // Black outline

        glBindVertexArray(selectionVAO);
        glDrawElements(GL_LINES, 24, GL_UNSIGNED_INT, 0);

        glDisable(GL_POLYGON_OFFSET_LINE);
        glLineWidth(1.0f);
        
        // Restore polygon mode
        if (wireframeMode)
          glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
        else
          glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
      }

      if (showDebugMenu)
      {
        ImGuiWindowFlags debugFlags = 0;
        if (mouseLocked)
          debugFlags |= ImGuiWindowFlags_NoInputs;
        ImGui::Begin("Debug", nullptr, debugFlags);
        
        if (ImGui::BeginTabBar("DebugTabs"))
        {
          if (ImGui::BeginTabItem("Info"))
          {
            ImGui::Text("FPS: %.1f", fps);
            ImGui::Text("Position: (%.2f, %.2f, %.2f)",
                        player.position.x, player.position.y, player.position.z);
            ImGui::Text("Velocity: (%.2f, %.2f, %.2f)",
                        player.velocity.x, player.velocity.y, player.velocity.z);
            ImGui::Text("Yaw: %.1f, Pitch: %.1f", player.yaw, player.pitch);
            ImGui::Text("On Ground: %s", player.onGround ? "Yes" : "No");

            int chunkX = static_cast<int>(floor(player.position.x / 16.0f));
            int chunkZ = static_cast<int>(floor(player.position.z / 16.0f));
            ImGui::Text("Chunk: (%d, %d)", chunkX, chunkZ);

            if (selectedBlock.has_value())
            {
              ImGui::Separator();
              ImGui::Text("Selected Block: (%d, %d, %d)", 
                  selectedBlock->blockPos.x, 
                  selectedBlock->blockPos.y, 
                  selectedBlock->blockPos.z);
              uint8_t blockId = getBlockAtWorld(
                  selectedBlock->blockPos.x, 
                  selectedBlock->blockPos.y, 
                  selectedBlock->blockPos.z, 
                  chunkManager);
              ImGui::Text("Block ID: %d", blockId);
              ImGui::Text("Distance: %.2f", selectedBlock->distance);
            }
            else
            {
              ImGui::Separator();
              ImGui::Text("No block selected");
            }

            ImGui::Separator();
            
            const char* blockNames[] = {"Air", "Dirt", "Grass", "Stone", "Sand", "Oak Log", "Oak Leaves", "Glass", "Oak Planks", "Water"};
            uint8_t selectedBlockId = PLACEABLE_BLOCKS[selectedBlockIndex];
            ImGui::Text("Selected: %s (ID: %d)", blockNames[selectedBlockId], selectedBlockId);
            ImGui::Text("Scroll wheel to change block");
            
            ImGui::Separator();
            ImGui::Text("LMB: Break block");
            ImGui::Text("RMB: Place block");
            ImGui::Text("Space: Jump");

            ImGui::Separator();
            ImGui::Text("Chunks loaded: %zu", chunkManager.chunks.size());
            ImGui::Text("Chunks loading: %zu", chunkManager.loadingChunks.size());
            ImGui::Text("Chunks meshing: %zu", chunkManager.meshingChunks.size());
            ImGui::Text("Jobs pending: %zu", jobSystem.pendingJobCount());
            
            ImGui::EndTabItem();
          }
          
          if (ImGui::BeginTabItem("Settings"))
          {
            ImGui::SliderInt("Render Distance", &renderDistance, 2, 16);
            
            ImGui::Separator();
            ImGui::Checkbox("Wireframe mode", &wireframeMode);
            ImGui::Checkbox("Noclip mode", &player.noclip);
            ImGui::Checkbox("Async Loading", &useAsyncLoading);
            ImGui::SliderFloat("Move Speed", &cameraSpeed, 0.0f, 60.0f);

            ImGui::Separator();
            ImGui::InputInt("Max FPS", &targetFps);
            if (targetFps < 10) targetFps = 10;
            if (targetFps > 1000) targetFps = 1000;

            ImGui::Separator();
            ImGui::Text("DAY/NIGHT CYCLE");
            ImGui::Checkbox("Auto Time", &autoTimeProgression);
            ImGui::SliderFloat("Time of Day", &worldTime, 0.0f, 1.0f, "%.2f");
            ImGui::SliderFloat("Day Length (s)", &dayLength, 60.0f, 1200.0f);
            ImGui::SliderFloat("Fog Density", &fogDensity, 0.001f, 0.05f, "%.4f");
            
            const char* timeNames[] = {"Midnight", "Dawn", "Morning", "Noon", "Afternoon", "Dusk", "Evening", "Night"};
            int timeIndex = static_cast<int>(worldTime * 8.0f) % 8;
            ImGui::Text("Current: %s (Sun: %.0f%%)", timeNames[timeIndex], rawSunBrightness * 100.0f);

            ImGui::Separator();
            ImGui::Text("WATER");
            ImGui::Checkbox("Caustics", &enableCaustics);
            if (isUnderwater)
            {
              ImGui::TextColored(ImVec4(0.2f, 0.6f, 1.0f, 1.0f), "UNDERWATER");
            }
            
            ImGui::EndTabItem();
          }
          
          ImGui::EndTabBar();
        }

        ImGui::End();

        ImGui::Begin("Fun Bullshit", nullptr, debugFlags);
        
        if (ImGui::Button("Randomize Block Textures"))
        {
          randomizeBlockTextures();
          for (auto& pair : chunkManager.chunks)
          {
            pair.second->dirtyMesh = true;
          }
        }
        
        if (ImGui::Button("Reset Block Textures"))
        {
          resetBlockTextures();
          for (auto& pair : chunkManager.chunks)
          {
            pair.second->dirtyMesh = true;
          }
        }

        ImGui::Separator();
        ImGui::Text("VISUAL CHAOS");

        ImGui::Checkbox("Drunk Mode", &drunkMode);
        if (drunkMode)
        {
          ImGui::SliderFloat("Drunk Intensity", &drunkIntensity, 0.1f, 5.0f);
        }

        ImGui::Checkbox("Disco Mode", &discoMode);
        if (discoMode)
        {
          ImGui::SliderFloat("Disco Speed", &discoSpeed, 1.0f, 50.0f);
        }

        ImGui::Checkbox("Earthquake", &earthquakeMode);
        if (earthquakeMode)
        {
          ImGui::SliderFloat("Quake Intensity", &earthquakeIntensity, 0.05f, 2.0f);
        }

        ImGui::End();
      }

      // Draw crosshair in the center of the screen
      ImDrawList* drawList = ImGui::GetForegroundDrawList();
      ImVec2 center(fbWidth * 0.5f, fbHeight * 0.5f);
      float crosshairSize = 10.0f;
      float crosshairThickness = 2.0f;
      ImU32 crosshairColor = IM_COL32(255, 255, 255, 200);
      
      // Horizontal line
      drawList->AddLine(
          ImVec2(center.x - crosshairSize, center.y),
          ImVec2(center.x + crosshairSize, center.y),
          crosshairColor, crosshairThickness);
      // Vertical line
      drawList->AddLine(
          ImVec2(center.x, center.y - crosshairSize),
          ImVec2(center.x, center.y + crosshairSize),
          crosshairColor, crosshairThickness);

      // Draw selected block icon in top-right corner
      uint8_t currentBlockId = PLACEABLE_BLOCKS[selectedBlockIndex];
      auto iconIt = g_blockIcons.find(currentBlockId);
      if (iconIt != g_blockIcons.end())
      {
          float iconSize = 128.0f;
          float padding = 20.0f;
          float iconX = fbWidth - iconSize - padding;
          float iconY = padding;
          
          // Draw background box
          float bgPadding = 8.0f;
          drawList->AddRectFilled(
              ImVec2(iconX - bgPadding, iconY - bgPadding),
              ImVec2(iconX + iconSize + bgPadding, iconY + iconSize + bgPadding),
              IM_COL32(0, 0, 0, 150),
              8.0f  // rounded corners
          );
          drawList->AddRect(
              ImVec2(iconX - bgPadding, iconY - bgPadding),
              ImVec2(iconX + iconSize + bgPadding, iconY + iconSize + bgPadding),
              IM_COL32(255, 255, 255, 100),
              8.0f,
              0,
              2.0f
          );
          
          // Draw the block icon
          drawList->AddImage(
              (ImTextureID)(intptr_t)iconIt->second,
              ImVec2(iconX, iconY),
              ImVec2(iconX + iconSize, iconY + iconSize)
          );
      }

      ImGui::Render();
      ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

      glfwSwapBuffers(window);

      if (targetFps < 1000)
      {
        limitFPS(targetFps);
      }

      glfwPollEvents();
    }

    jobSystem.stop();
    std::cout << "Job system stopped" << std::endl;

    PlayerData playerToSave;
    playerToSave.x = player.position.x;
    playerToSave.y = player.position.y;
    playerToSave.z = player.position.z;
    playerToSave.yaw = player.yaw;
    playerToSave.pitch = player.pitch;
    playerToSave.timeOfDay = worldTime;
    regionManager.savePlayerData(playerToSave);
    std::cout << "Player position saved" << std::endl;

    for (auto &pair : chunkManager.chunks)
    {
      Chunk* chunk = pair.second.get();
      regionManager.saveChunkData(chunk->position.x, chunk->position.y, chunk->position.z, chunk->blocks);
    }
    regionManager.flush();
    std::cout << "World saved" << std::endl;

    chunkManager.chunks.clear();
    
    glDeleteVertexArrays(1, &selectionVAO);
    glDeleteBuffers(1, &selectionVBO);
    glDeleteBuffers(1, &selectionEBO);
    selectionShader.Delete();
    waterShader.Delete();

    unloadBlockIcons();

    ImGui_ImplGlfw_Shutdown();
    ImGui_ImplOpenGL3_Shutdown();
    ImGui::DestroyContext();

    shaderProgram.Delete();

    glfwDestroyWindow(window);
    glfwTerminate();

#ifdef _WIN32
    timeEndPeriod(1);
#endif

    return 0;
  }
  catch (const std::exception &ex)
  {
    std::cerr << "Fatal error: " << ex.what() << std::endl;
#ifdef _WIN32
    timeEndPeriod(1);
#endif
    return EXIT_FAILURE;
  }
}

void framebuffer_size_callback(GLFWwindow *window, int width, int height)
{
  glViewport(0, 0, width, height);
  // Reset mouse tracking to prevent camera jump when window is resized/maximized
  firstMouse = true;
}

void processInput(GLFWwindow *window, Player &player, float dt)
{
  if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
    glfwSetWindowShouldClose(window, true);

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

  // Build input direction from WASD keys
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

  // Handle noclip mode for vertical movement
  if (player.noclip)
  {
    if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS)
      inputDir.y += 1.0f;
    if (glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS)
      inputDir.y -= 1.0f;
  }

  player.applyMovement(inputDir, cameraSpeed);

  // Handle jumping (only in normal mode)
  if (!player.noclip && glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS)
  {
    player.jump();
  }
}

std::string resolveTexturePath(const std::string &relativePath)
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

void mouseButtonCallback(GLFWwindow *window, int button, int action, int mods)
{
  // Ignore if ImGui wants mouse input
  ImGuiIO& io = ImGui::GetIO();
  if (io.WantCaptureMouse)
    return;

  // Only handle clicks when mouse is locked
  if (!mouseLocked)
    return;

  if (action != GLFW_PRESS)
    return;

  if (!g_player || !g_chunkManager)
    return;

  // Create a temporary Camera for CameraForward
  Camera tempCam{g_player->getEyePosition(), g_player->yaw, g_player->pitch, 70.0f};
  glm::vec3 forward = CameraForward(tempCam);
  glm::vec3 eyePos = g_player->getEyePosition();
  auto hit = raycastVoxel(eyePos, forward, MAX_RAYCAST_DISTANCE, *g_chunkManager);

  if (!hit.has_value())
    return;

  if (button == GLFW_MOUSE_BUTTON_LEFT)
  {
    // Break block (set to air)
    setBlockAtWorld(hit->blockPos.x, hit->blockPos.y, hit->blockPos.z, 0, *g_chunkManager);
  }
  else if (button == GLFW_MOUSE_BUTTON_RIGHT)
  {
    // Place block (on the face we hit)
    glm::ivec3 placePos = hit->blockPos + hit->normal;
    
    // Don't place if it would be inside the player's AABB
    AABB playerAABB = g_player->getAABB();
    AABB blockAABB = AABB::fromBlockPos(placePos.x, placePos.y, placePos.z);
    
    if (playerAABB.intersects(blockAABB))
      return;  // Would place inside player
    
    // Place the currently selected block
    uint8_t blockToPlace = PLACEABLE_BLOCKS[selectedBlockIndex];
    setBlockAtWorld(placePos.x, placePos.y, placePos.z, blockToPlace, *g_chunkManager);
  }
}

void scrollCallback(GLFWwindow *window, double xoffset, double yoffset)
{
  // Ignore if ImGui wants mouse input
  ImGuiIO& io = ImGui::GetIO();
  if (io.WantCaptureMouse)
    return;

  // Only handle scroll when mouse is locked
  if (!mouseLocked)
    return;

  // Scroll up = next block, scroll down = previous block
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
