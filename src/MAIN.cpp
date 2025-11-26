#include "../libs/imgui/backends/imgui_impl_glfw.h"
#include "../libs/imgui/backends/imgui_impl_opengl3.h"
#include "../libs/imgui/imgui.h"
#include "EBO.h"
#include "ShaderClass.h"
#include "VAO.h"
#include "VBO.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#include <GLFW/glfw3.h>
#include <cstdlib>
#include <exception>
#include <filesystem>
#include <glad/glad.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <iostream>
#include <string>
#include <unordered_map>
#include <cstdint>

struct Camera
{
  glm::vec3 position;
  float yaw;
  float pitch;
  float fov;
};

glm::vec3 CameraForward(const Camera &camera)
{
  glm::vec3 forward;
  forward.x = cos(glm::radians(camera.yaw)) * cos(glm::radians(camera.pitch));
  forward.y = sin(glm::radians(camera.pitch));
  forward.z = sin(glm::radians(camera.yaw)) * cos(glm::radians(camera.pitch));
  return glm::normalize(forward);
}

// chunks
using BlockID = uint8_t;
constexpr int CHUNK_SIZE = 16;
constexpr int CHUNK_VOLUME = CHUNK_SIZE * CHUNK_SIZE * CHUNK_SIZE;

struct Chunk
{
  glm::ivec3 position;
  BlockID blocks[CHUNK_VOLUME];

  bool dirtyMesh = true;
  GLuint vao, vbo, ebo;
};

// chunk manager
inline int64_t chunkKey(int x, int y, int z)
{
  return ((int64_t)x << 42) ^ ((int64_t)y << 21) ^ (int64_t)z;
}

struct ChunkManager
{
  std::unordered_map<int64_t, Chunk *> chunks;

  Chunk *getChunk(int cx, int cy, int cz);
  bool hasChunk(int cx, int cy, int cz);
  Chunk *loadChunk(int cx, int cy, int cz);
  void unloadChunk(int cx, int cy, int cz);
};

bool ChunkManager::hasChunk(int cx, int cy, int cz)
{
  return chunks.find(chunkKey(cx, cy, cz)) != chunks.end();
}

Chunk *ChunkManager::getChunk(int cx, int cy, int cz)
{
  auto it = chunks.find(chunkKey(cx, cy, cz));
  if (it == chunks.end())
    return nullptr;
  return it->second;
}

Chunk *ChunkManager::loadChunk(int cx, int cy, int cz)
{
  if (hasChunk(cx, cy, cz))
    return getChunk(cx, cy, cz);

  std::cout << "Loading chunk at (" << cx << ", " << cy << ", " << cz << ")" << std::endl;

  Chunk *c = new Chunk();
  c->position = {cx, cy, cz};

  // fill with something simple for now
  for (int i = 0; i < CHUNK_VOLUME; i++)
    c->blocks[i] = 0; // air

  chunks[chunkKey(cx, cy, cz)] = c;
  return c;
}

void ChunkManager::unloadChunk(int cx, int cy, int cz)
{
  auto key = chunkKey(cx, cy, cz);
  auto it = chunks.find(key);

  if (it != chunks.end())
  {
    delete it->second; // free memory
    chunks.erase(it);
  }
}

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

float vertices[] = {
    // coords 3            // colors 3    // textures 2
    -0.5f, -0.5f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, // lower left
    0.5f, 0.5f, 0.0f, 1.0f, 1.0f, 0.0f, 1.0f, 1.0f,   // upper right
    -0.5f, 0.5f, 0.0f, 1.0f, 1.0f, 0.0f, 0.0f, 1.0f,  // upper left
    0.5f, -0.5f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f, 0.0f   // lower right
};
unsigned int indices[] = {
    // note that we start from 0!
    0, 1, 2, // first triangle
    0, 3, 1  // second triangle
};

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

    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
    // Ensure a valid viewport before the first resize event fires.
    int fbWidth = 0, fbHeight = 0;
    glfwGetFramebufferSize(window, &fbWidth, &fbHeight);
    glViewport(0, 0, fbWidth, fbHeight);

    Shader shaderProgram("default.vert", "default.frag");
    shaderProgram.Activate();
    glUniform1i(glGetUniformLocation(shaderProgram.ID, "ourTexture"), 0);

    VAO VAO1;
    VAO1.Bind();

    // Generates Vertex Buffer Object and links it to vertices
    VBO VBO1(vertices, sizeof(vertices));
    // Generates Element Buffer Object and links it to indices
    EBO EBO1(indices, sizeof(indices));

    // Links VBO attributes such as coordinates and colors to VAO
    // https://youtu.be/45MIykWJ-C4?t=2508
    VAO1.LinkAttrib(VBO1, 0, 3, GL_FLOAT, 8 * sizeof(float), (void *)0);
    VAO1.LinkAttrib(VBO1, 1, 3, GL_FLOAT, 8 * sizeof(float),
                    (void *)(3 * sizeof(float)));
    VAO1.LinkAttrib(VBO1, 2, 2, GL_FLOAT, 8 * sizeof(float),
                    (void *)(6 * sizeof(float)));
    // Unbind all to prevent accidentally modifying them
    VAO1.Unbind();
    VBO1.Unbind();
    EBO1.Unbind();

    stbi_set_flip_vertically_on_load(true);

    unsigned int texture;
    glGenTextures(1, &texture);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER,
                    GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    int width = 0, height = 0, nrChannels = 0;
    std::string texturePath =
        resolveTexturePath("assets/textures/container.jpg");
    unsigned char *data =
        stbi_load(texturePath.c_str(), &width, &height, &nrChannels, 0);

    if (data)
    {

      glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_RGB,
                   GL_UNSIGNED_BYTE, data);
      glGenerateMipmap(GL_TEXTURE_2D);
    }
    else
    {
      std::cerr << "Failed to load texture at " << texturePath << ": "
                << stbi_failure_reason() << std::endl;
      // Fallback magenta texture so we can still see geometry.
      unsigned char fallback[] = {255, 0, 255};
      glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 1, 1, 0, GL_RGB, GL_UNSIGNED_BYTE,
                   fallback);
    }
    stbi_image_free(data);

    // Gets ID of uniform called "scale"
    GLuint uniID = glGetUniformLocation(shaderProgram.ID, "scale");

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
      glClear(GL_COLOR_BUFFER_BIT);

      shaderProgram.Activate();
      glUniform1f(uniID, 0.2f);
      glBindTexture(GL_TEXTURE_2D, texture);
      VAO1.Bind();

      if (wireframeMode)
        glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
      else
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

      ImGui_ImplOpenGL3_NewFrame();
      ImGui_ImplGlfw_NewFrame();
      ImGui::NewFrame();

      glm::mat4 model = glm::mat4(1.0f);

      glm::vec3 camForward = CameraForward(cam);
      glm::mat4 view = glm::lookAt(
          cam.position,
          cam.position + camForward,
          glm::vec3(0.0f, 1.0f, 0.0f));

      float aspect = static_cast<float>(fbWidth) / static_cast<float>(fbHeight);
      glm::mat4 proj = glm::perspective(glm::radians(cam.fov), aspect, 0.1f, 1000.f);

      glm::mat4 mvp = proj * view * model;
      unsigned int transformLoc =
          glGetUniformLocation(shaderProgram.ID, "transform");
      glUniformMatrix4fv(transformLoc, 1, GL_FALSE, glm::value_ptr(mvp));

      int cx = floor(cam.position.x / CHUNK_SIZE);
      int cy = 0; // for now
      int cz = floor(cam.position.z / CHUNK_SIZE);

      const int LOAD_RADIUS = 4;

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

      glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);

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
    ImGui_ImplGlfw_Shutdown();
    ImGui_ImplOpenGL3_Shutdown();
    ImGui::DestroyContext();

    VAO1.Delete();
    VBO1.Delete();
    EBO1.Delete();
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
