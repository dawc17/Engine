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

#include "../rendering/Camera.h"
#include "../rendering/Meshing.h"
#include "../rendering/ParticleSystem.h"

#include "../world/Chunk.h"
#include "../world/ChunkManager.h"
#include "../world/WaterSimulator.h"

#include "../utils/BlockTypes.h"
#include "../utils/CoordUtils.h"

#include "../world/Biome.h"
#include "../world/TerrainGenerator.h"

#include "../gameplay/Raycast.h"
#include "../gameplay/Player.h"
#include "../gameplay/SurvivalSystem.h"
#include "../gameplay/Inventory.h"

#include "MainGlobals.h"
#include "GameState.h"
#include "WorldSession.h"
#include "Renderer.h"

#include "../ui/MainMenu.h"
#include "../ui/DebugUI.h"
#include "../ui/HUD.h"

#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <algorithm>
#include <cstdlib>
#include <exception>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <iostream>
#include <string>
#include <vector>
#include <thread>


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

    glfwWindowHint(GLFW_MAXIMIZED, GLFW_TRUE);
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
    int fbWidth = 0, fbHeight = 0;
    glfwGetFramebufferSize(window, &fbWidth, &fbHeight);
    glViewport(0, 0, fbWidth, fbHeight);

    Renderer renderer;
    renderer.init();

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO &io = ImGui::GetIO();
    (void)io;
    ImGui::StyleColorsDark();
    
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

    WorldSession session;

    ParticleSystem particleSystem;
    particleSystem.init();

    g_player = &player;
    g_particleSystem = &particleSystem;

    auto& chunkManager  = session.chunkManager;
    auto& jobSystem      = session.jobSystem;
    auto& waterSimulator = session.waterSimulator;
    auto& regionManager  = session.regionManager;
    auto& selectedBlock  = session.selectedBlock;
    std::vector<glm::ivec2> loadOffsets;
    int cachedLoadRadius = -1;

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

        drawChat(player, window);

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

        FrameParams fp{};
        fp.view = view;
        fp.proj = proj;
        fp.eyePos = eyePos;
        fp.skyColor = skyColor;
        fp.fogCol = fogCol;
        fp.clearCol = clearCol;
        fp.aspect = aspect;
        fp.sunBrightness = sunBrightness;
        fp.ambientLight = ambientLight;
        fp.effectiveFogDensity = effectiveFogDensity;
        fp.gameTime = static_cast<float>(glfwGetTime());

        renderer.beginFrame(fp);

        int cx = static_cast<int>(std::floor(player.position.x / CHUNK_SIZE));
        int cz = static_cast<int>(std::floor(player.position.z / CHUNK_SIZE));

        const int LOAD_RADIUS = renderDistance;
        const int UNLOAD_RADIUS = LOAD_RADIUS + 2;
        const int CHUNK_HEIGHT_MIN = 0;
        const int CHUNK_HEIGHT_MAX = (256 / CHUNK_SIZE) - 1;
        if (cachedLoadRadius != LOAD_RADIUS)
        {
          loadOffsets.clear();
          for (int dx = -LOAD_RADIUS; dx <= LOAD_RADIUS; dx++)
          {
            for (int dz = -LOAD_RADIUS; dz <= LOAD_RADIUS; dz++)
            {
              loadOffsets.push_back({dx, dz});
            }
          }
          std::sort(loadOffsets.begin(), loadOffsets.end(),
              [](const glm::ivec2& a, const glm::ivec2& b)
              {
                int da = a.x * a.x + a.y * a.y;
                int db = b.x * b.x + b.y * b.y;
                return da < db;
              });
          cachedLoadRadius = LOAD_RADIUS;
        }
        size_t pendingJobs = jobSystem ? jobSystem->pendingJobCount() : 0;
        int maxLoadEnqueuePerFrame = 32;
        int maxMeshEnqueuePerFrame = 16;
        if (pendingJobs > 200)
        {
          maxLoadEnqueuePerFrame = 4;
          maxMeshEnqueuePerFrame = 0;
        }
        else if (pendingJobs > 140)
        {
          maxLoadEnqueuePerFrame = 8;
          maxMeshEnqueuePerFrame = 2;
        }
        else if (pendingJobs > 90)
        {
          maxLoadEnqueuePerFrame = 12;
          maxMeshEnqueuePerFrame = 4;
        }
        else if (pendingJobs > 50)
        {
          maxLoadEnqueuePerFrame = 20;
          maxMeshEnqueuePerFrame = 8;
        }

        if (currentState == GameState::Playing)
        {
          int enqueuedLoads = 0;
          for (const glm::ivec2& offset : loadOffsets)
          {
            if (useAsyncLoading && enqueuedLoads >= maxLoadEnqueuePerFrame)
              break;
            int chunkX = cx + offset.x;
            int chunkZ = cz + offset.y;
            for (int cy = CHUNK_HEIGHT_MIN; cy <= CHUNK_HEIGHT_MAX; cy++)
            {
              if (useAsyncLoading && enqueuedLoads >= maxLoadEnqueuePerFrame)
                break;
              if (!chunkManager->hasChunk(chunkX, cy, chunkZ) &&
                  !chunkManager->isLoading(chunkX, cy, chunkZ) &&
                  !chunkManager->isSaving(chunkX, cy, chunkZ))
              {
                if (useAsyncLoading)
                {
                  chunkManager->enqueueLoadChunk(chunkX, cy, chunkZ);
                  enqueuedLoads++;
                }
                else
                  chunkManager->loadChunk(chunkX, cy, chunkZ);
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

        std::vector<std::pair<int, Chunk*>> meshCandidates;
        meshCandidates.reserve(chunkManager->chunks.size());
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
              int dx = chunk->position.x - cx;
              int dz = chunk->position.z - cz;
              int dist2 = dx * dx + dz * dz;
              meshCandidates.push_back({dist2, chunk});
            }
          }
        }
        std::sort(meshCandidates.begin(), meshCandidates.end(),
            [](const std::pair<int, Chunk*>& a, const std::pair<int, Chunk*>& b)
            {
              return a.first < b.first;
            });
        int enqueuedMeshes = 0;
        for (const auto& candidate : meshCandidates)
        {
          Chunk* chunk = candidate.second;
          if (useAsyncLoading)
          {
            if (enqueuedMeshes >= maxMeshEnqueuePerFrame)
              break;
            chunkManager->enqueueMeshChunk(chunk->position.x, chunk->position.y, chunk->position.z);
            enqueuedMeshes++;
          }
          else
          {
            buildChunkMesh(*chunk, *chunkManager);
            chunk->dirtyMesh = false;
          }
        }

        renderer.renderChunks(fp, *chunkManager);
        renderer.renderWater(fp, *chunkManager);
        renderer.renderParticles(particleSystem, fp);

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
                    glm::vec4 particleTint(1.0f);
                    if (g_blockTypes[player.breakingBlockId].faceTint[0])
                    {
                      BiomeID biome = getBiomeAt(hit->blockPos.x, hit->blockPos.z);
                      bool isLeaf = g_blockTypes[player.breakingBlockId].transparent && g_blockTypes[player.breakingBlockId].solid;
                      glm::vec3 t = isLeaf ? getBiomeFoliageTint(biome) : getBiomeGrassTint(biome);
                      particleTint = glm::vec4(t, 1.0f);
                    }
                    g_particleSystem->spawnBlockBreakParticles(blockCenter, tileIndex, skyLightVal, 15, particleTint);
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

        renderer.renderSelectionBox(fp, selectedBlock);

        if (selectedBlock.has_value() && player.isBreaking &&
            selectedBlock->blockPos == player.breakingBlockPos)
          renderer.renderDestroyOverlay(fp, player, *chunkManager);

        if (currentState == GameState::Playing)
          renderer.renderHeldItem(fp, player);

        drawDebugUI(player, chunkManager.get(), jobSystem.get(),
            waterSimulator.get(), selectedBlock, rawSunBrightness, useAsyncLoading);

        if (currentState == GameState::Playing)
            drawGameHUD(player, fbWidth, fbHeight, inventoryOpen);
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
          session.init(result.selectedWorld, result.gamemode, player, window, numWorkers);
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
          session.shutdown(player, window);
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

    session.shutdown(player, window);

    renderer.cleanup();

    ImGui_ImplGlfw_Shutdown();
    ImGui_ImplOpenGL3_Shutdown();
    ImGui::DestroyContext();

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

