#include "../../libs/glm-1.0.2/glm/fwd.hpp"
#ifdef _WIN32
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <mmsystem.h>
#pragma comment(lib, "winmm.lib")
#endif

#include "../../libs/imgui/backends/imgui_impl_glfw.h"
#include "../../libs/imgui/backends/imgui_impl_opengl3.h"
#include "../../libs/imgui/imgui.h"
#include "../rendering/opengl/ShaderClass.h"
#include "../rendering/Camera.h"
#include "../world/Chunk.h"
#include "../world/ChunkManager.h"
#include "../rendering/Meshing.h"
#include "../utils/BlockTypes.h"
#include "../utils/CoordUtils.h"
#include "../gameplay/Raycast.h"
#include "../gameplay/Player.h"
#include "../utils/JobSystem.h"
#include "../world/RegionManager.h"
#include "../world/WaterSimulator.h"
#include "../rendering/ParticleSystem.h"
#include "MainGlobals.h"
#include "../gameplay/SurvivalSystem.h"
#include "../gameplay/Inventory.h"
#include "GameState.h"
#include "../rendering/ItemModelGenerator.h"
#include "../ui/MainMenu.h"
#include <memory>

#include "../thirdparty/stb_image.h"

#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <algorithm>
#include <array>
#include <cstdlib>
#include <exception>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <iostream>
#include <string>
#include <vector>
#include <thread>
#include <unordered_map>


int main(int argc, char* argv[])
{
  try
  {
#ifdef _WIN32
    timeBeginPeriod(1);
#endif

    for (int i = 1; i < argc; i++)
    {
      if (std::string(argv[i]) == "--debug")
      {
        debugEnabled = true;
        showDebugMenu = true;
      }
    }

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
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);

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
    loadItemModels();

    GLuint destroyTextures[10] = {};
    for (int i = 0; i < 10; ++i)
    {
      std::string destroyPath = resolveTexturePath(
          "assets/textures/destroy/destroy_stage_" + std::to_string(i) + ".png");
      destroyTextures[i] = loadHUDIcon(destroyPath, true);
    }

    Shader selectionShader("selection.vert", "selection.frag");
    GLint selectionTransformLoc = glGetUniformLocation(selectionShader.ID, "transform");
    GLint selectionColorLoc = glGetUniformLocation(selectionShader.ID, "color");

    Shader destroyShader("destroy.vert", "destroy.frag");
    destroyShader.Activate();
    glUniform1i(glGetUniformLocation(destroyShader.ID, "crackTex"), 0);
    GLint destroyTransformLoc = glGetUniformLocation(destroyShader.ID, "transform");
    GLint destroyTimeOfDayLoc = glGetUniformLocation(destroyShader.ID, "timeOfDay");
    GLint destroyAmbientLightLoc = glGetUniformLocation(destroyShader.ID, "ambientLight");
    GLint destroySkyLightLoc = glGetUniformLocation(destroyShader.ID, "SkyLight");
    GLint destroyFaceShadeLoc = glGetUniformLocation(destroyShader.ID, "FaceShade");

    Shader itemModelShader("item_model.vert", "item_model.frag");
    itemModelShader.Activate();
    glUniform1i(glGetUniformLocation(itemModelShader.ID, "textureArray"), 0);
    GLint itemTransformLoc = glGetUniformLocation(itemModelShader.ID, "transform");
    GLint itemTimeOfDayLoc = glGetUniformLocation(itemModelShader.ID, "timeOfDay");
    GLint itemAmbientLightLoc = glGetUniformLocation(itemModelShader.ID, "ambientLight");

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

    float faceVertices[] = {
        0.0f, 0.0f, 0.0f, 0.0f, 0.0f,
        1.0f, 0.0f, 0.0f, 1.0f, 0.0f,
        1.0f, 1.0f, 0.0f, 1.0f, 1.0f,
        0.0f, 1.0f, 0.0f, 0.0f, 1.0f
    };
    unsigned int faceIndices[] = {
        0, 1, 2, 2, 3, 0
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

    GLuint faceVAO, faceVBO, faceEBO;
    glGenVertexArrays(1, &faceVAO);
    glGenBuffers(1, &faceVBO);
    glGenBuffers(1, &faceEBO);

    glBindVertexArray(faceVAO);
    glBindBuffer(GL_ARRAY_BUFFER, faceVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(faceVertices), faceVertices, GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, faceEBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(faceIndices), faceIndices, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);
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

    bool useAsyncLoading = true;

    Player player;
    initSurvival(player);

    float lastFrame = 0.0f;

    glm::vec3 respawnPos(0.0f, 120.0f, 0.0f);

    int numWorkers = (std::max)(2, static_cast<int>(std::thread::hardware_concurrency()) - 1);

    std::unique_ptr<RegionManager> regionManager;
    std::unique_ptr<JobSystem> jobSystem;
    std::unique_ptr<ChunkManager> chunkManager;
    std::unique_ptr<WaterSimulator> waterSimulator;

    ParticleSystem particleSystem;
    particleSystem.init();

    g_player = &player;
    g_particleSystem = &particleSystem;

    std::optional<RaycastHit> selectedBlock;

    auto initWorld = [&](const std::string& worldName)
    {
      currentWorldName = worldName;
      std::string worldPath = "saves/" + worldName;

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
        player.gamemode = Gamemode::Survival;
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
        }
      }

      mouseLocked = true;
      glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
      firstMouse = true;
      currentState = GameState::Playing;
    };

    auto shutdownWorld = [&]()
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
    };

    while (!glfwWindowShouldClose(window))
    {
      float currentFrame = static_cast<float>(glfwGetTime());
      float deltaTime = currentFrame - lastFrame;
      lastFrame = currentFrame;

      const float MAX_DELTA_TIME = 0.05f;
      if (deltaTime > MAX_DELTA_TIME)
        deltaTime = MAX_DELTA_TIME;

      fps = 1.0f / deltaTime;

      glfwGetFramebufferSize(window, &fbWidth, &fbHeight);
      if (fbHeight == 0)
        fbHeight = 1;

      static bool escWasPressed = false;
      bool escPressed = glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS;
      bool escJustPressed = escPressed && !escWasPressed;
      escWasPressed = escPressed;

      if (escJustPressed && !chatOpen)
      {
        if (inventoryOpen)
        {
          if (!player.inventory.heldItem.isEmpty())
          {
            player.inventory.addItem(player.inventory.heldItem.blockId, player.inventory.heldItem.count);
            player.inventory.heldItem.clear();
          }
          inventoryOpen = false;
          mouseLocked = true;
          glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
          firstMouse = true;
        }
        else
        {
          switch (currentState)
          {
          case GameState::Playing:
            currentState = GameState::Paused;
            mouseLocked = false;
            glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
            break;
          case GameState::Paused:
            currentState = GameState::Playing;
            mouseLocked = true;
            glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
            firstMouse = true;
            break;
          case GameState::WorldSelect:
            currentState = GameState::MainMenu;
            break;
          case GameState::Settings:
            currentState = settingsReturnState;
            if (settingsReturnState == GameState::Playing)
            {
              mouseLocked = true;
              glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
              firstMouse = true;
            }
            break;
          case GameState::MainMenu:
            glfwSetWindowShouldClose(window, true);
            break;
          }
        }
      }

      ImGui_ImplOpenGL3_NewFrame();
      ImGui_ImplGlfw_NewFrame();
      ImGui::NewFrame();

      if (chunkManager)
      {
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

        if (currentState == GameState::Playing)
        {
          xoffset *= mouseSensitivity;
          yoffset *= mouseSensitivity;

          if (mouseLocked && !player.isDead)
          {
            player.yaw += xoffset;
            player.pitch += yoffset;
          }

          if (player.pitch > 89.0f)
            player.pitch = 89.0f;
          if (player.pitch < -89.0f)
            player.pitch = -89.0f;

          if (player.isDead)
          {
            player.velocity = glm::vec3(0.0f);
          }
          else
          {
            processInput(window, player, deltaTime);
            player.update(deltaTime, *chunkManager);
          }

          if (autoTimeProgression)
          {
            worldTime += deltaTime / dayLength;
            if (worldTime > 1.0f) worldTime -= 1.0f;
          }

          glm::vec3 eyeCheck = player.getEyePosition();
          uint8_t blockAtEye = getBlockAtWorld(
              static_cast<int>(floor(eyeCheck.x)),
              static_cast<int>(floor(eyeCheck.y)),
              static_cast<int>(floor(eyeCheck.z)),
              *chunkManager);
          isUnderwater = (blockAtEye == 9);
          updateSurvival(player, deltaTime, isUnderwater);
          if (player.isDead)
          {
            if (glfwGetKey(window, GLFW_KEY_R) == GLFW_PRESS)
              respawnPlayer(player, respawnPos);
          }

          if (enableWaterSimulation)
          {
            waterTickAccumulator += deltaTime;
            while (waterTickAccumulator >= WATER_TICK_INTERVAL)
            {
              waterSimulator->tick();
              waterTickAccumulator -= WATER_TICK_INTERVAL;
            }
          }

          particleSystem.update(deltaTime);
        }

        chunkManager->update();

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
          skyColor = dayColor;
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

        if (chatOpen && currentState == GameState::Playing)
        {
          ImGui::SetNextWindowPos(ImVec2(20.0f, 20.0f), ImGuiCond_Always);
          ImGui::SetNextWindowSize(ImVec2(600.0f, 200.0f), ImGuiCond_Always);
          ImGui::Begin("Chat", nullptr, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize);

          ImGui::BeginChild("ChatLog", ImVec2(0.0f, -ImGui::GetFrameHeightWithSpacing()), false);
          for (const auto& line : chatLog)
            ImGui::TextUnformatted(line.c_str());
          ImGui::EndChild();

          if (chatFocusNext)
          {
            ImGui::SetKeyboardFocusHere();
            chatFocusNext = false;
          }

          if (ImGui::InputText("##ChatInput", chatInput, sizeof(chatInput), ImGuiInputTextFlags_EnterReturnsTrue))
          {
            std::string input(chatInput);
            chatInput[0] = 0;
            executeCommand(input, player);
            chatOpen = false;
            mouseLocked = true;
            glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
            firstMouse = true;
          }

          ImGui::End();
        }

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

        int cx = static_cast<int>(std::floor(player.position.x / CHUNK_SIZE));
        int cz = static_cast<int>(std::floor(player.position.z / CHUNK_SIZE));

        const int LOAD_RADIUS = renderDistance;
        const int UNLOAD_RADIUS = LOAD_RADIUS + 2;
        const int CHUNK_HEIGHT_MIN = 0;
        const int CHUNK_HEIGHT_MAX = (256 / CHUNK_SIZE) - 1;

        if (currentState == GameState::Playing)
        {
          for (int dx = -LOAD_RADIUS; dx <= LOAD_RADIUS; dx++)
          {
            for (int dz = -LOAD_RADIUS; dz <= LOAD_RADIUS; dz++)
            {
              for (int cy = CHUNK_HEIGHT_MIN; cy <= CHUNK_HEIGHT_MAX; cy++)
              {
                int chunkX = cx + dx;
                int chunkZ = cz + dz;

                if (!chunkManager->hasChunk(chunkX, cy, chunkZ) &&
                    !chunkManager->isLoading(chunkX, cy, chunkZ) &&
                    !chunkManager->isSaving(chunkX, cy, chunkZ))
                {
                  if (useAsyncLoading)
                    chunkManager->enqueueLoadChunk(chunkX, cy, chunkZ);
                  else
                    chunkManager->loadChunk(chunkX, cy, chunkZ);
                }
              }
            }
          }

          std::vector<ChunkManager::ChunkCoord> toUnload;
          for (auto& pair : chunkManager->chunks)
          {
            Chunk* chunk = pair.second.get();
            int distX = chunk->position.x - cx;
            int distZ = chunk->position.z - cz;
            if (std::abs(distX) > UNLOAD_RADIUS || std::abs(distZ) > UNLOAD_RADIUS)
            {
              if (!chunkManager->isSaving(chunk->position.x, chunk->position.y, chunk->position.z))
                toUnload.push_back({chunk->position.x, chunk->position.y, chunk->position.z});
            }
          }
          for (const auto& coord : toUnload)
          {
            if (useAsyncLoading)
              chunkManager->enqueueSaveAndUnload(coord.x, coord.y, coord.z);
            else
              chunkManager->unloadChunk(coord.x, coord.y, coord.z);
          }
        }

        for (auto& pair : chunkManager->chunks)
        {
          Chunk* chunk = pair.second.get();
          if (chunk->dirtyMesh && !chunkManager->isMeshing(chunk->position.x, chunk->position.y, chunk->position.z))
          {
            bool neighborsReady = true;
            for (int i = 0; i < 6; i++)
            {
              int nx = chunk->position.x + DIRS[i].x;
              int ny = chunk->position.y + DIRS[i].y;
              int nz = chunk->position.z + DIRS[i].z;
              if (ny >= CHUNK_HEIGHT_MIN && ny <= CHUNK_HEIGHT_MAX)
              {
                if (!chunkManager->hasChunk(nx, ny, nz))
                {
                  if (chunkManager->isLoading(nx, ny, nz))
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
                chunkManager->enqueueMeshChunk(chunk->position.x, chunk->position.y, chunk->position.z);
              else
              {
                buildChunkMesh(*chunk, *chunkManager);
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

        waterShader.Activate();
        glBindTexture(GL_TEXTURE_2D_ARRAY, textureArray);

        float gameTime = static_cast<float>(glfwGetTime());
        glUniform1f(waterTimeLoc, gameTime);
        glUniform1f(waterTimeOfDayLoc, sunBrightness);
        glUniform3fv(waterCameraPosLoc, 1, glm::value_ptr(eyePos));
        glUniform3fv(waterSkyColorLoc, 1, glm::value_ptr(skyColor));
        glUniform3fv(waterFogColorLoc, 1, glm::value_ptr(fogCol));
        glUniform1f(waterFogDensityLoc, effectiveFogDensity);
        glUniform1f(waterAmbientLightLoc, ambientLight);
        glUniform1i(waterEnableCausticsLoc, enableCaustics ? 1 : 0);

        for (auto& pair : chunkManager->chunks)
        {
          Chunk* chunk = pair.second.get();
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
        glBindTexture(GL_TEXTURE_2D_ARRAY, textureArray);
        particleSystem.render(view, proj, eyePos, sunBrightness, ambientLight);
        glDisable(GL_BLEND);

        if (currentState == GameState::Playing)
        {
          selectedBlock = raycastVoxel(eyePos, camForward, MAX_RAYCAST_DISTANCE, *chunkManager);

          if (player.isBreaking && player.gamemode == Gamemode::Survival)
          {
            auto hit = raycastVoxel(eyePos, camForward, MAX_RAYCAST_DISTANCE, *chunkManager);
            if (!hit.has_value())
            {
              player.isBreaking = false;
            }
            else
            {
              if (hit->blockPos != player.breakingBlockPos)
              {
                player.isBreaking = false;
              }
              else
              {
                float hardness = getBlockHardness(player.breakingBlockId);
                player.breakProgress += deltaTime / hardness;
                if (player.breakProgress >= 1.0f)
                {
                  setBlockAtWorld(hit->blockPos.x, hit->blockPos.y, hit->blockPos.z, 0, *chunkManager);
                  player.inventory.addItem(player.breakingBlockId, 1);

                  if (g_waterSimulator)
                    g_waterSimulator->onBlockChanged(hit->blockPos.x, hit->blockPos.y, hit->blockPos.z, player.breakingBlockId, 0);

                  if (g_particleSystem && player.breakingBlockId != 0)
                  {
                    int tileIndex = g_blockTypes[player.breakingBlockId].faceTexture[0];
                    glm::vec3 blockCenter = glm::vec3(hit->blockPos) + glm::vec3(0.5f);
                    float skyLightVal = 1.0f;
                    {
                      glm::ivec3 cpos = worldToChunk(hit->blockPos.x, hit->blockPos.y, hit->blockPos.z);
                      Chunk* c = chunkManager->getChunk(cpos.x, cpos.y, cpos.z);
                      if (c)
                      {
                        glm::ivec3 local = worldToLocal(hit->blockPos.x, hit->blockPos.y, hit->blockPos.z);
                        skyLightVal = static_cast<float>(c->skyLight[blockIndex(local.x, local.y, local.z)]) / static_cast<float>(MAX_SKY_LIGHT);
                      }
                    }
                    g_particleSystem->spawnBlockBreakParticles(blockCenter, tileIndex, skyLightVal, 15);
                  }

                  player.isBreaking = false;
                  player.breakProgress = 0.0f;
                }
              }
            }
          }
          else
          {
            player.breakProgress = 0.0f;
          }

          if (player.isDead || player.gamemode != Gamemode::Survival)
            player.isBreaking = false;
        }

        if (selectedBlock.has_value())
        {
          glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
          glLineWidth(2.0f);
          glEnable(GL_POLYGON_OFFSET_LINE);
          glPolygonOffset(-1.0f, -1.0f);

          selectionShader.Activate();
          glm::mat4 selectionModel = glm::translate(glm::mat4(1.0f),
              glm::vec3(selectedBlock->blockPos));
          glm::mat4 selectionMVP = proj * view * selectionModel;
          glUniformMatrix4fv(selectionTransformLoc, 1, GL_FALSE, glm::value_ptr(selectionMVP));
          glUniform4f(selectionColorLoc, 0.0f, 0.0f, 0.0f, 1.0f);

          glBindVertexArray(selectionVAO);
          glDrawElements(GL_LINES, 24, GL_UNSIGNED_INT, 0);

          glDisable(GL_POLYGON_OFFSET_LINE);
          glLineWidth(1.0f);

          if (wireframeMode)
            glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
          else
            glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
        }

        if (player.isBreaking && selectedBlock.has_value() &&
            selectedBlock->blockPos == player.breakingBlockPos)
        {
          int stage = static_cast<int>(player.breakProgress * 10.0f);
          if (stage < 0) stage = 0;
          if (stage > 9) stage = 9;
          GLuint crackTex = destroyTextures[stage];

          if (crackTex != 0)
          {
            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            glDepthMask(GL_FALSE);

            destroyShader.Activate();
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, crackTex);

            glUniform1f(destroyTimeOfDayLoc, sunBrightness);
            glUniform1f(destroyAmbientLightLoc, ambientLight);

            float skyLightVal = 1.0f;
            {
              glm::ivec3 cpos = worldToChunk(player.breakingBlockPos.x, player.breakingBlockPos.y, player.breakingBlockPos.z);
              Chunk* c = chunkManager->getChunk(cpos.x, cpos.y, cpos.z);
              if (c)
              {
                glm::ivec3 local = worldToLocal(player.breakingBlockPos.x, player.breakingBlockPos.y, player.breakingBlockPos.z);
                skyLightVal = static_cast<float>(c->skyLight[blockIndex(local.x, local.y, local.z)]) / static_cast<float>(MAX_SKY_LIGHT);
              }
            }
            glUniform1f(destroySkyLightLoc, skyLightVal);

            glm::vec3 blockPos = glm::vec3(player.breakingBlockPos);
            const float offset = 0.001f;

            const float faceShades[6] = {
                FACE_SHADE[DIR_POS_Z],
                FACE_SHADE[DIR_NEG_Z],
                FACE_SHADE[DIR_POS_Y],
                FACE_SHADE[DIR_NEG_Y],
                FACE_SHADE[DIR_POS_X],
                FACE_SHADE[DIR_NEG_X]
            };

            std::array<glm::mat4, 6> faceTransforms = {
                glm::translate(glm::mat4(1.0f), blockPos + glm::vec3(0, 0, 1 + offset)),
                glm::translate(glm::mat4(1.0f), blockPos + glm::vec3(1, 0, -offset)) * glm::rotate(glm::mat4(1.0f), glm::radians(180.0f), glm::vec3(0, 1, 0)),
                glm::translate(glm::mat4(1.0f), blockPos + glm::vec3(0, 1 + offset, 1)) * glm::rotate(glm::mat4(1.0f), glm::radians(-90.0f), glm::vec3(1, 0, 0)),
                glm::translate(glm::mat4(1.0f), blockPos + glm::vec3(0, -offset, 0)) * glm::rotate(glm::mat4(1.0f), glm::radians(90.0f), glm::vec3(1, 0, 0)),
                glm::translate(glm::mat4(1.0f), blockPos + glm::vec3(1 + offset, 0, 1)) * glm::rotate(glm::mat4(1.0f), glm::radians(90.0f), glm::vec3(0, 1, 0)),
                glm::translate(glm::mat4(1.0f), blockPos + glm::vec3(-offset, 0, 0)) * glm::rotate(glm::mat4(1.0f), glm::radians(-90.0f), glm::vec3(0, 1, 0))
            };

            glBindVertexArray(faceVAO);
            for (size_t i = 0; i < faceTransforms.size(); ++i)
            {
              const glm::mat4& ft = faceTransforms[i];
              glUniform1f(destroyFaceShadeLoc, faceShades[i]);
              glm::mat4 mvpFace = proj * view * ft;
              glUniformMatrix4fv(destroyTransformLoc, 1, GL_FALSE, glm::value_ptr(mvpFace));
              glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
            }
            glBindVertexArray(0);

            glDepthMask(GL_TRUE);
            glDisable(GL_BLEND);
          }
        }

        if (currentState == GameState::Playing && !inventoryOpen)
        {
            const ItemStack& held = player.inventory.selectedItem();
            if (!held.isEmpty())
            {
                auto it = g_itemModels.find(held.blockId);
                if (it != g_itemModels.end() && it->second.indexCount > 0)
                {
                    glClear(GL_DEPTH_BUFFER_BIT);
                    if (wireframeMode)
                        glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
                    else
                        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

                    itemModelShader.Activate();
                    glActiveTexture(GL_TEXTURE0);
                    glBindTexture(GL_TEXTURE_2D_ARRAY, textureArray);
                    glUniform1f(itemTimeOfDayLoc, sunBrightness);
                    glUniform1f(itemAmbientLightLoc, ambientLight);

                    glm::mat4 handProj = glm::perspective(
                        glm::radians(70.0f), aspect, 0.01f, 10.0f);

                    glm::mat4 handModel = glm::mat4(1.0f);
                    handModel = glm::translate(handModel,
                        glm::vec3(0.4f, -0.36f, -0.7f));
                    handModel = glm::rotate(handModel,
                        glm::radians(35.0f), glm::vec3(0.0f, 1.0f, 0.0f));
                    handModel = glm::rotate(handModel,
                        glm::radians(-20.0f), glm::vec3(1.0f, 0.0f, 0.0f));
                    handModel = glm::scale(handModel, glm::vec3(0.16f));
                    handModel = glm::translate(handModel,
                        glm::vec3(-0.5f, -0.5f, -0.5f));

                    glm::mat4 handMVP = handProj * handModel;
                    glUniformMatrix4fv(itemTransformLoc, 1, GL_FALSE,
                        glm::value_ptr(handMVP));

                    glBindVertexArray(it->second.vao);
                    glDrawElements(GL_TRIANGLES, it->second.indexCount,
                        GL_UNSIGNED_INT, 0);
                    glBindVertexArray(0);
                }
            }
        }

        if (showDebugMenu && currentState == GameState::Playing)
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

              int chunkXDbg = static_cast<int>(floor(player.position.x / 16.0f));
              int chunkZDbg = static_cast<int>(floor(player.position.z / 16.0f));
              ImGui::Text("Chunk: (%d, %d)", chunkXDbg, chunkZDbg);

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
                    *chunkManager);
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
              const ItemStack& heldBlock = player.inventory.selectedItem();
              if (!heldBlock.isEmpty() && heldBlock.blockId < 10)
                ImGui::Text("Holding: %s x%d", blockNames[heldBlock.blockId], heldBlock.count);
              else if (!heldBlock.isEmpty())
                ImGui::Text("Holding: Block %d x%d", heldBlock.blockId, heldBlock.count);
              else
                ImGui::Text("Holding: Empty");
              ImGui::Text("Slot: %d/9", player.inventory.selectedHotbar + 1);

              ImGui::Separator();
              ImGui::Text("LMB: Break block");
              ImGui::Text("RMB: Place block");
              ImGui::Text("Space: Jump");

              ImGui::Separator();
              ImGui::Text("Chunks loaded: %zu", chunkManager->chunks.size());
              ImGui::Text("Chunks loading: %zu", chunkManager->loadingChunks.size());
              ImGui::Text("Chunks meshing: %zu", chunkManager->meshingChunks.size());
              ImGui::Text("Jobs pending: %zu", jobSystem->pendingJobCount());

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
              ImGui::Checkbox("Water Physics", &enableWaterSimulation);
              if (enableWaterSimulation)
              {
                ImGui::SliderInt("Water Tick Rate", &waterTickRate, 1, 20);
                waterSimulator->setTickRate(waterTickRate);
              }
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
            for (auto& pair : chunkManager->chunks)
              pair.second->dirtyMesh = true;
          }

          if (ImGui::Button("Reset Block Textures"))
          {
            resetBlockTextures();
            for (auto& pair : chunkManager->chunks)
              pair.second->dirtyMesh = true;
          }

          ImGui::Separator();
          ImGui::Text("VISUAL CHAOS");

          ImGui::Checkbox("Drunk Mode", &drunkMode);
          if (drunkMode)
            ImGui::SliderFloat("Drunk Intensity", &drunkIntensity, 0.1f, 5.0f);

          ImGui::Checkbox("Disco Mode", &discoMode);
          if (discoMode)
            ImGui::SliderFloat("Disco Speed", &discoSpeed, 1.0f, 50.0f);

          ImGui::Checkbox("Earthquake", &earthquakeMode);
          if (earthquakeMode)
            ImGui::SliderFloat("Quake Intensity", &earthquakeIntensity, 0.05f, 2.0f);

          ImGui::End();
        }

        if (currentState == GameState::Playing)
        {
          ImDrawList* drawList = ImGui::GetForegroundDrawList();

          if (player.gamemode == Gamemode::Survival)
            drawSurvivalHud(player, fbWidth, fbHeight);

          if (!inventoryOpen)
          {
            ImVec2 center(fbWidth * 0.5f, fbHeight * 0.5f);
            float crosshairSize = 10.0f;
            float crosshairThickness = 2.0f;
            ImU32 crosshairColor = IM_COL32(255, 255, 255, 200);

            drawList->AddLine(
                ImVec2(center.x - crosshairSize, center.y),
                ImVec2(center.x + crosshairSize, center.y),
                crosshairColor, crosshairThickness);
            drawList->AddLine(
                ImVec2(center.x, center.y - crosshairSize),
                ImVec2(center.x, center.y + crosshairSize),
                crosshairColor, crosshairThickness);
          }

          drawHotbar(player.inventory, fbWidth, fbHeight);
          if (inventoryOpen)
            drawInventoryScreen(player.inventory, fbWidth, fbHeight);
        }
      }
      else
      {
        glClearColor(0.08f, 0.08f, 0.12f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
      }

      switch (currentState)
      {
      case GameState::MainMenu:
      {
        auto result = drawMainMenu(fbWidth, fbHeight);
        if (result.shouldQuit)
          glfwSetWindowShouldClose(window, true);
        else if (result.nextState != GameState::MainMenu)
        {
          if (result.nextState == GameState::Settings)
            settingsReturnState = GameState::MainMenu;
          currentState = result.nextState;
        }
        break;
      }
      case GameState::WorldSelect:
      {
        auto result = drawWorldSelect(fbWidth, fbHeight);
        if (result.nextState == GameState::Playing && !result.selectedWorld.empty())
        {
          initWorld(result.selectedWorld);
        }
        else if (result.nextState != GameState::WorldSelect)
        {
          currentState = result.nextState;
        }
        break;
      }
      case GameState::Paused:
      {
        auto result = drawPauseMenu(fbWidth, fbHeight);
        if (result.nextState == GameState::MainMenu)
        {
          shutdownWorld();
          currentState = GameState::MainMenu;
        }
        else if (result.nextState == GameState::Settings)
        {
          settingsReturnState = GameState::Paused;
          currentState = GameState::Settings;
        }
        else if (result.nextState == GameState::Playing)
        {
          currentState = GameState::Playing;
          mouseLocked = true;
          glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
          firstMouse = true;
        }
        break;
      }
      case GameState::Settings:
      {
        auto result = drawSettings(fbWidth, fbHeight, settingsReturnState);
        if (result.nextState != GameState::Settings)
        {
          currentState = result.nextState;
          if (result.nextState == GameState::Playing)
          {
            mouseLocked = true;
            glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
            firstMouse = true;
          }
        }
        break;
      }
      case GameState::Playing:
        break;
      }

      ImGui::Render();
      ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

      glfwSwapBuffers(window);

      if (targetFps < 1000)
        limitFPS(targetFps);

      glfwPollEvents();
    }

    shutdownWorld();

    glDeleteVertexArrays(1, &selectionVAO);
    glDeleteBuffers(1, &selectionVBO);
    glDeleteBuffers(1, &selectionEBO);
    glDeleteVertexArrays(1, &faceVAO);
    glDeleteBuffers(1, &faceVBO);
    glDeleteBuffers(1, &faceEBO);
    selectionShader.Delete();
    destroyShader.Delete();
    waterShader.Delete();
    itemModelShader.Delete();

    for (GLuint tex : destroyTextures)
    {
      if (tex != 0)
        glDeleteTextures(1, &tex);
    }
    unloadBlockIcons();
    unloadItemModels();

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

