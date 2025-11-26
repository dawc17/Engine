#include "../libs/imgui/backends/imgui_impl_glfw.h"
#include "../libs/imgui/backends/imgui_impl_opengl3.h"
#include "../libs/imgui/imgui.h"
#include "ShaderClass.h"
#include "Camera.h"
#include "Chunk.h"
#include "ChunkManager.h"
#include "Meshing.h"
#include "BlockTypes.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#include <GLFW/glfw3.h>
#include <algorithm>
#include <cstdlib>
#include <exception>
#include <filesystem>
#include <glad/glad.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <iostream>
#include <string>
#include <tuple>
#include <vector>

void framebuffer_size_callback(GLFWwindow *window, int width, int height);
void processInput(GLFWwindow *window, Camera &camera, float dt);
std::string resolveTexturePath(const std::string &relativePath);

const int SCREEN_WIDTH = 1280;
const int SCREEN_HEIGHT = 720;

float fps = 0.0f;
float cameraSpeed = 2.5f;

bool mouseLocked = true;
bool firstMouse = true;
double lastMouseX = SCREEN_WIDTH / 2.0;
double lastMouseY = SCREEN_HEIGHT / 2.0;

int main()
{
  try
  {
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow *window = glfwCreateWindow(SCREEN_WIDTH, SCREEN_HEIGHT,
                                          "LearnOpenGL", NULL, NULL);
    if (window == NULL)
    {
      std::cout << "Failed to initialize (bruh?)" << std::endl;
      glfwTerminate();
      return 1;
    }
    glfwMakeContextCurrent(window);
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

    gladLoadGL();

    // Enable depth testing
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

    stbi_set_flip_vertically_on_load(false);

    // Load atlas and convert to 2D texture array (each tile becomes a layer)
    unsigned int textureArray;
    glGenTextures(1, &textureArray);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D_ARRAY, textureArray);
    
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_NEAREST_MIPMAP_LINEAR);
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

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO &io = ImGui::GetIO();
    (void)io;
    ImGui::StyleColorsDark();
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 460");

    bool wireframeMode = false;

    Camera cam{
        glm::vec3(0.0f, 0.0f, 3.0f),
        -90.0f,
        0.0f,
        70.0f};

    float lastFrame = 0.0f;

    ChunkManager chunkManager;

    // main draw loop sigma
    while (!glfwWindowShouldClose(window))
    {
      float currentFrame = glfwGetTime();
      float deltaTime = currentFrame - lastFrame;
      lastFrame = currentFrame;

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
        cam.yaw += xoffset;
        cam.pitch += yoffset;
      }

      if (cam.pitch > 89.0f)
        cam.pitch = 89.0f;
      if (cam.pitch < -89.0f)
        cam.pitch = -89.0f;

      fps = 1.0f / deltaTime;

      processInput(window, cam, deltaTime);

      glClearColor(0.2f, 0.3f, 0.3f, 1.0f);
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

      glm::vec3 camForward = CameraForward(cam);
      glm::mat4 view = glm::lookAt(
          cam.position,
          cam.position + camForward,
          glm::vec3(0.0f, 1.0f, 0.0f));

      float aspect = static_cast<float>(fbWidth) / static_cast<float>(fbHeight);
      glm::mat4 proj = glm::perspective(glm::radians(cam.fov), aspect, 0.1f, 1000.f);

      glm::mat4 mvp = proj * view * model;
      glUniformMatrix4fv(transformLoc, 1, GL_FALSE, glm::value_ptr(mvp));

      int cx = floor(cam.position.x / CHUNK_SIZE);
      int cy = 0; // for now
      int cz = floor(cam.position.z / CHUNK_SIZE);

      const int LOAD_RADIUS = 4;
      const int UNLOAD_RADIUS = LOAD_RADIUS + 2;

      // Load chunks within radius
      for (int dx = -LOAD_RADIUS; dx <= LOAD_RADIUS; dx++)
      {
        for (int dz = -LOAD_RADIUS; dz <= LOAD_RADIUS; dz++)
        {
          int chunkX = cx + dx;
          int chunkZ = cz + dz;

          if (!chunkManager.hasChunk(chunkX, cy, chunkZ))
          {
            chunkManager.loadChunk(chunkX, cy, chunkZ);
          }
        }
      }

      // unload chunks outside unload radius
      std::vector<ChunkManager::ChunkCoord> toUnload;
      for (auto &pair : chunkManager.chunks)
      {
        Chunk *chunk = pair.second.get();
        int distX = chunk->position.x - cx;
        int distZ = chunk->position.z - cz;
        if (std::abs(distX) > UNLOAD_RADIUS || std::abs(distZ) > UNLOAD_RADIUS)
        {
          toUnload.push_back({chunk->position.x, chunk->position.y, chunk->position.z});
        }
      }
      for (const auto &coord : toUnload)
      {
        chunkManager.unloadChunk(coord.x, coord.y, coord.z);
      }

      // render chunks
      for (auto &pair : chunkManager.chunks)
      {
        Chunk *chunk = pair.second.get();
        if (chunk->dirtyMesh)
        {
          buildChunkMesh(*chunk);
          chunk->dirtyMesh = false;
        }

        if (chunk->indexCount > 0)
        {
          glm::mat4 chunkModel = glm::translate(glm::mat4(1.0f), glm::vec3(chunk->position.x * CHUNK_SIZE, chunk->position.y * CHUNK_SIZE, chunk->position.z * CHUNK_SIZE));
          glm::mat4 chunkMVP = proj * view * chunkModel;
          glUniformMatrix4fv(transformLoc, 1, GL_FALSE, glm::value_ptr(chunkMVP));

          glBindVertexArray(chunk->vao);
          glDrawElements(GL_TRIANGLES, chunk->indexCount, GL_UNSIGNED_INT, 0);
        }
      }

      ImGui::Begin("Debug");
      ImGui::Text("FPS: %.1f", fps);
      ImGui::Text("Camera pos: (%.2f, %.2f, %.2f)",
                  cam.position.x, cam.position.y, cam.position.z);
      ImGui::Text("Yaw: %.1f, Pitch: %.1f", cam.yaw, cam.pitch);

      int chunkX = static_cast<int>(floor(cam.position.x / 16.0f));
      int chunkZ = static_cast<int>(floor(cam.position.z / 16.0f));
      ImGui::Text("Chunk: (%d, %d)", chunkX, chunkZ);

      ImGui::Checkbox("Wireframe mode", &wireframeMode);
      ImGui::SliderFloat("Camera Speed", &cameraSpeed, 0.0f, 10.0f);

      ImGui::End();

      ImGui::Render();
      ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

      glfwSwapBuffers(window);
      glfwPollEvents();
    }

    // Release GPU resources for chunks before the OpenGL context is torn down.
    chunkManager.chunks.clear();
    ImGui_ImplGlfw_Shutdown();
    ImGui_ImplOpenGL3_Shutdown();
    ImGui::DestroyContext();

    shaderProgram.Delete();

    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
  }
  catch (const std::exception &ex)
  {
    std::cerr << "Fatal error: " << ex.what() << std::endl;
    return EXIT_FAILURE;
  }
}

void framebuffer_size_callback(GLFWwindow *window, int width, int height)
{
  glViewport(0, 0, width, height);
}

void processInput(GLFWwindow *window, Camera &camera, float dt)
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

  float speed = cameraSpeed * dt;

  glm::vec3 forward = CameraForward(camera);
  glm::vec3 right = glm::normalize(glm::cross(forward, glm::vec3(0.0f, 1.0f, 0.0f)));

  if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
    camera.position += forward * speed;
  if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
    camera.position -= forward * speed;
  if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
    camera.position -= right * speed;
  if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
    camera.position += right * speed;
}

std::string resolveTexturePath(const std::string &relativePath)
{
  namespace fs = std::filesystem;
  fs::path direct(relativePath);
  if (fs::exists(direct))
  {
    return direct.string();
  }

  fs::path fromBuild = fs::path("..") / relativePath;
  if (fs::exists(fromBuild))
  {
    return fromBuild.string();
  }

  return relativePath;
}
