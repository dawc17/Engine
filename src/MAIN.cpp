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
  uint32_t indexCount = 0;
  uint32_t vertexCount = 0;
};

constexpr glm::ivec3 DIRS[6] = {
    {1, 0, 0},  // +X
    {-1, 0, 0}, // -X
    {0, 1, 0},  // +Y
    {0, -1, 0}, // -Y
    {0, 0, 1},  // +Z
    {0, 0, -1}  // -Z
};

inline int blockIndex(int x, int y, int z)
{
  return x + CHUNK_SIZE * (y + CHUNK_SIZE * z);
}

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

  glGenVertexArrays(1, &c->vao);
  glGenBuffers(1, &c->vbo);
  glGenBuffers(1, &c->ebo);

  // fill with something simple for now
    for (int y = 0; y < CHUNK_SIZE; y++) {
      for (int x = 0; x < CHUNK_SIZE; x++) {
        for (int z = 0; z < CHUNK_SIZE; z++) {
            int i = blockIndex(x, y, z);
            if (y < 8)
                c->blocks[i] = 1; // dirt
            else
                c->blocks[i] = 0; // air
        }
    }
}


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

// meshing
struct Vertex
{
  glm::vec3 pos;
  glm::vec2 uv;
};

void uploadToGPU(Chunk &c, const std::vector<Vertex> &verts, const std::vector<uint32_t> &inds);

static const Vertex FACE_POS_X[4] = { {{1, 0, 0}, {1, 0}}, {{1, 1, 0}, {1, 1}}, {{1, 1, 1}, {0, 1}}, {{1, 0, 1}, {0, 0}} };
static const Vertex FACE_NEG_X[4] = { {{0, 0, 1}, {1, 0}}, {{0, 1, 1}, {1, 1}}, {{0, 1, 0}, {0, 1}}, {{0, 0, 0}, {0, 0}} };
static const Vertex FACE_POS_Y[4] = { {{0, 1, 0}, {1, 0}}, {{0, 1, 1}, {1, 1}}, {{1, 1, 1}, {0, 1}}, {{1, 1, 0}, {0, 0}} };
static const Vertex FACE_NEG_Y[4] = { {{0, 0, 1}, {1, 0}}, {{0, 0, 0}, {1, 1}}, {{1, 0, 0}, {0, 1}}, {{1, 0, 1}, {0, 0}} };
static const Vertex FACE_POS_Z[4] = { {{1, 0, 1}, {1, 0}}, {{1, 1, 1}, {1, 1}}, {{0, 1, 1}, {0, 1}}, {{0, 0, 1}, {0, 0}} };
static const Vertex FACE_NEG_Z[4] = { {{0, 0, 0}, {1, 0}}, {{0, 1, 0}, {1, 1}}, {{1, 1, 0}, {0, 1}}, {{1, 0, 0}, {0, 0}} };

static const Vertex *FACE_TABLE[6] = {
    FACE_POS_X, FACE_NEG_X,
    FACE_POS_Y, FACE_NEG_Y,
    FACE_POS_Z, FACE_NEG_Z
};

static const uint32_t FACE_INDICES[6] = {
    0, 1, 2,
    0, 2, 3};

void buildChunkMesh(Chunk &c)
{
  auto getBlock = [&](int x, int y, int z) -> BlockID
  {
    if (x < 0 || x >= CHUNK_SIZE ||
        y < 0 || y >= CHUNK_SIZE ||
        z < 0 || z >= CHUNK_SIZE)
      return 0; // treat out-of-bounds as air for now
    return c.blocks[blockIndex(x, y, z)];
  };

  std::vector<Vertex> verts;
  std::vector<uint32_t> inds;

  // Greedy meshing
  for (int dir = 0; dir < 6; dir++)
  {
    glm::ivec3 n = DIRS[dir];
    int axis = 0;
    if (n.y != 0) axis = 1;
    if (n.z != 0) axis = 2;

    int u = (axis + 1) % 3;
    int v = (axis + 2) % 3;

    // 2D mask for the slice
    BlockID mask[CHUNK_SIZE][CHUNK_SIZE];
    
    for (int i = 0; i < CHUNK_SIZE; i++)
    {
      // 1. Compute mask
      for (int j = 0; j < CHUNK_SIZE; j++) // v
      {
        for (int k = 0; k < CHUNK_SIZE; k++) // u
        {
          glm::ivec3 pos;
          pos[axis] = i;
          pos[u] = k;
          pos[v] = j;

          BlockID current = c.blocks[blockIndex(pos.x, pos.y, pos.z)];
          
          glm::ivec3 npos = pos + n;
          BlockID neighbor = getBlock(npos.x, npos.y, npos.z);

          if (current != 0 && neighbor == 0)
          {
            mask[j][k] = current;
          }
          else
          {
            mask[j][k] = 0;
          }
        }
      }

      // 2. Greedy meshing on mask
      for (int j = 0; j < CHUNK_SIZE; j++)
      {
        for (int k = 0; k < CHUNK_SIZE; k++)
        {
          if (mask[j][k] != 0)
          {
            BlockID type = mask[j][k];
            int w = 1;
            int h = 1;

            // Compute width
            while (k + w < CHUNK_SIZE && mask[j][k + w] == type)
            {
              w++;
            }

            // Compute height
            bool done = false;
            while (j + h < CHUNK_SIZE)
            {
              for (int dx = 0; dx < w; dx++)
              {
                if (mask[j + h][k + dx] != type)
                {
                  done = true;
                  break;
                }
              }
              if (done) break;
              h++;
            }

            // Add quad
            const Vertex *face = FACE_TABLE[dir];
            uint32_t baseIndex = verts.size();

            for (int vIdx = 0; vIdx < 4; vIdx++)
            {
              Vertex vtx = face[vIdx];
              glm::vec3 finalPos;

              // Axis (normal)
              finalPos[axis] = i + vtx.pos[axis];

              // U axis
              if (vtx.pos[u] > 0.5f) finalPos[u] = k + w;
              else finalPos[u] = k;

              // V axis
              if (vtx.pos[v] > 0.5f) finalPos[v] = j + h;
              else finalPos[v] = j;

              vtx.pos = finalPos;
              
              // Scale UVs
              if (vtx.uv.x > 0.5f) vtx.uv.x = w;
              if (vtx.uv.y > 0.5f) vtx.uv.y = h;

              verts.push_back(vtx);
            }

            for (int idx = 0; idx < 6; idx++)
              inds.push_back(baseIndex + FACE_INDICES[idx]);

            // Clear mask
            for (int dy = 0; dy < h; dy++)
            {
              for (int dx = 0; dx < w; dx++)
              {
                mask[j + dy][k + dx] = 0;
              }
            }
          }
        }
      }
    }
  }

  uploadToGPU(c, verts, inds);
}

void uploadToGPU(Chunk &c,
                 const std::vector<Vertex> &verts,
                 const std::vector<uint32_t> &inds)
{
  glBindVertexArray(c.vao);

  glBindBuffer(GL_ARRAY_BUFFER, c.vbo);
  glBufferData(GL_ARRAY_BUFFER,
               verts.size() * sizeof(Vertex),
               verts.data(),
               GL_STATIC_DRAW);

  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, c.ebo);
  glBufferData(GL_ELEMENT_ARRAY_BUFFER,
               inds.size() * sizeof(uint32_t),
               inds.data(),
               GL_STATIC_DRAW);

  glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex),
                        (void *)offsetof(Vertex, pos));
  glEnableVertexAttribArray(0);

  glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex),
                        (void *)offsetof(Vertex, uv));
  glEnableVertexAttribArray(1);

  c.indexCount = inds.size();
  c.vertexCount = verts.size();
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

      for (auto &[key, chunk] : chunkManager.chunks)
      {
        if (chunk->dirtyMesh)
        {
          buildChunkMesh(*chunk);
          chunk->dirtyMesh = false;
        }

        glBindVertexArray(chunk->vao);
        glDrawElements(GL_TRIANGLES, chunk->indexCount,
                       GL_UNSIGNED_INT, 0);
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
