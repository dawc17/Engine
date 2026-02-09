#include "ToolModelGenerator.h"
#include "../core/MainGlobals.h"
#include "../thirdparty/stb_image.h"
#include "Meshing.h"
#include "glm/ext/vector_float3.hpp"
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <unordered_map>
#include <vector>


std::unordered_map<uint8_t, ToolModel> g_toolModels;
std::unordered_map<uint8_t, GLuint> g_toolIcons;

ToolModel ToolModelGenerator::generateFromSprite(const std::string &imagePath) {
  int width, height, channels;
  unsigned char *data =
      stbi_load(imagePath.c_str(), &width, &height, &channels, 4);
  if (!data) {
    std::cerr << "Failed to load tool sprite: " << imagePath << std::endl;
    return {};
  }

  std::vector<bool> solid(width * height, false);
  std::vector<glm::vec3> colors(width * height);

  for (int y = 0; y < height; y++) {
    for (int x = 0; x < width; x++) {
      int idx = (y * width + x) * 4;
      unsigned char a = data[idx + 3];
      if (a > 127) {
        solid[y * width + x] = true;
        colors[y * width + x] =
            glm::vec3(data[idx + 0] / 255.0f, data[idx + 1] / 255.0f,
                      data[idx + 2] / 255.0f);
      }
    }
  }

  stbi_image_free(data);

  auto isSolid = [&](int x, int y) -> bool {
    if (x < 0 || x >= width || y < 0 || y >= height)
      return false;
    return solid[y * width + x];
  };

  std::vector<ToolVertex> vertices;
  std::vector<uint32_t> indices;

  float pixelSize = 1.0f / static_cast<float>(width);
  float depth = pixelSize;

  for (int py = 0; py < height; py++) {
    for (int px = 0; px < width; px++) {
      if (!solid[py * width + px])
        continue;

      glm::vec3 color = colors[py * width + px];

      float x0 = px * pixelSize;
      float x1 = (px + 1) * pixelSize;
      float y0 = (height - 1 - py) * pixelSize;
      float y1 = (height - py) * pixelSize;
      float z0 = 0.0f;
      float z1 = depth;

      if (!isSolid(px + 1, py)) {
        uint32_t base = static_cast<uint32_t>(vertices.size());
        vertices.push_back({glm::vec3(x1, y0, z1), color, FACE_SHADE[0]});
        vertices.push_back({glm::vec3(x1, y1, z1), color, FACE_SHADE[0]});
        vertices.push_back({glm::vec3(x1, y1, z0), color, FACE_SHADE[0]});
        vertices.push_back({glm::vec3(x1, y0, z0), color, FACE_SHADE[0]});
        indices.insert(indices.end(),
                       {base, base + 1, base + 2, base, base + 2, base + 3});
      }

      if (!isSolid(px - 1, py)) {
        uint32_t base = static_cast<uint32_t>(vertices.size());
        vertices.push_back({glm::vec3(x0, y0, z0), color, FACE_SHADE[1]});
        vertices.push_back({glm::vec3(x0, y1, z0), color, FACE_SHADE[1]});
        vertices.push_back({glm::vec3(x0, y1, z1), color, FACE_SHADE[1]});
        vertices.push_back({glm::vec3(x0, y0, z1), color, FACE_SHADE[1]});
        indices.insert(indices.end(),
                       {base, base + 1, base + 2, base, base + 2, base + 3});
      }

      if (!isSolid(px, py - 1)) {
        uint32_t base = static_cast<uint32_t>(vertices.size());
        vertices.push_back({glm::vec3(x0, y1, z1), color, FACE_SHADE[2]});
        vertices.push_back({glm::vec3(x1, y1, z1), color, FACE_SHADE[2]});
        vertices.push_back({glm::vec3(x1, y1, z0), color, FACE_SHADE[2]});
        vertices.push_back({glm::vec3(x0, y1, z0), color, FACE_SHADE[2]});
        indices.insert(indices.end(),
                       {base, base + 1, base + 2, base, base + 2, base + 3});
      }

      if (!isSolid(px, py + 1)) {
        uint32_t base = static_cast<uint32_t>(vertices.size());
        vertices.push_back({glm::vec3(x0, y0, z0), color, FACE_SHADE[3]});
        vertices.push_back({glm::vec3(x1, y0, z0), color, FACE_SHADE[3]});
        vertices.push_back({glm::vec3(x1, y0, z1), color, FACE_SHADE[3]});
        vertices.push_back({glm::vec3(x0, y0, z1), color, FACE_SHADE[3]});
        indices.insert(indices.end(),
                       {base, base + 1, base + 2, base, base + 2, base + 3});
      }

      {
        uint32_t base = static_cast<uint32_t>(vertices.size());
        vertices.push_back({glm::vec3(x0, y0, z1), color, FACE_SHADE[4]});
        vertices.push_back({glm::vec3(x0, y1, z1), color, FACE_SHADE[4]});
        vertices.push_back({glm::vec3(x1, y1, z1), color, FACE_SHADE[4]});
        vertices.push_back({glm::vec3(x1, y0, z1), color, FACE_SHADE[4]});
        indices.insert(indices.end(),
                       {base, base + 1, base + 2, base, base + 2, base + 3});
      }

      {
        uint32_t base = static_cast<uint32_t>(vertices.size());
        vertices.push_back({glm::vec3(x1, y0, z0), color, FACE_SHADE[5]});
        vertices.push_back({glm::vec3(x1, y1, z0), color, FACE_SHADE[5]});
        vertices.push_back({glm::vec3(x0, y1, z0), color, FACE_SHADE[5]});
        vertices.push_back({glm::vec3(x0, y0, z0), color, FACE_SHADE[5]});
        indices.insert(indices.end(),
                       {base, base + 1, base + 2, base, base + 2, base + 3});
      }
    }
  }

  if (vertices.empty())
    return {};

  ToolModel model;

  glGenVertexArrays(1, &model.vao);
  glGenBuffers(1, &model.vbo);
  glGenBuffers(1, &model.ebo);

  glBindVertexArray(model.vao);

  glBindBuffer(GL_ARRAY_BUFFER, model.vbo);
  glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(ToolVertex),
               vertices.data(), GL_STATIC_DRAW);

  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, model.ebo);
  glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(uint32_t),
               indices.data(), GL_STATIC_DRAW);

  glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(ToolVertex),
                        (void *)0);
  glEnableVertexAttribArray(0);

  glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(ToolVertex),
                        (void *)offsetof(ToolVertex, color));
  glEnableVertexAttribArray(1);

  glVertexAttribPointer(2, 1, GL_FLOAT, GL_FALSE, sizeof(ToolVertex),
                        (void *)offsetof(ToolVertex, faceShade));
  glEnableVertexAttribArray(2);

  glBindVertexArray(0);

  model.indexCount = static_cast<uint32_t>(indices.size());
  return model;
}

void ToolModelGenerator::destroyModel(ToolModel &model)
{
    if (model.vao) glDeleteVertexArrays(1, &model.vao);
    if (model.vbo) glDeleteBuffers(1, &model.vbo);
    if (model.ebo) glDeleteBuffers(1, &model.ebo);
    model = {};
}

void loadToolModels()
{
    struct ToolDef {
        uint8_t id;
        std::string spritePath;
    };

    ToolDef tools[] = {
        {TOOL_DIAMOND_PICKAXE, "assets/textures/tools/diamond_pickaxe.png"},
    };

    for (const auto &tool : tools)
    {
        std::string resolvedSprite = resolveTexturePath(tool.spritePath);
        ToolModel model = ToolModelGenerator::generateFromSprite(resolvedSprite);
        if (model.indexCount > 0)
        {
            g_toolModels[tool.id] = model;
            std::cout << "Generated tool model for tool " << (int)tool.id << std::endl;
        }

        GLuint icon = loadHUDIcon(resolvedSprite, true);
        if (icon != 0)
            g_toolIcons[tool.id] = icon;
    }
}

void unloadToolModels()
{
    for (auto& [id, model] : g_toolModels)
        ToolModelGenerator::destroyModel(model);
    g_toolModels.clear();

    for (auto& [id, tex] : g_toolIcons)
        glDeleteTextures(1, &tex);
    g_toolIcons.clear();
}