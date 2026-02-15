#pragma once
#include <glad/glad.h>
#include <glm/glm.hpp>
#include <string>
#include <vector>
#include <unordered_map>
#include <cstdint>

struct ToolVertex {
    glm::vec3 pos;
    glm::vec3 color;
    float faceShade;
};

struct ToolModel {
    GLuint vao = 0;
    GLuint vbo = 0;
    GLuint ebo = 0;
    uint32_t indexCount = 0;
};

constexpr uint8_t TOOL_ID_START = 128;

// pickaxes
constexpr uint8_t TOOL_DIAMOND_PICKAXE = 128;
constexpr uint8_t TOOL_WOOD_PICKAXE = 129;
constexpr uint8_t TOOL_STONE_PICKAXE = 130;
constexpr uint8_t TOOL_GOLD_PICKAXE = 131;
constexpr uint8_t TOOL_IRON_PICKAXE = 132;

inline bool isToolItem(uint8_t id) { return id >= TOOL_ID_START; }

class ToolModelGenerator {
    public:
    static ToolModel generateFromSprite(const unsigned char* pngData, unsigned int pngSize);
    static void destroyModel(ToolModel& model);
};

extern std::unordered_map<uint8_t, ToolModel> g_toolModels;
extern std::unordered_map<uint8_t, GLuint> g_toolIcons;

void loadToolModels();
void unloadToolModels();
