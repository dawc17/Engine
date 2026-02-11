#pragma once
#include <glad/glad.h>
#include <glm/glm.hpp>
#include <string>
#include <vector>
#include <unordered_map>
#include <cstdint>

struct ItemVertex {
    glm::vec3 pos;
    glm::vec2 uv;
    float tileIndex;
    float faceShade;
    glm::vec3 biomeTint;
};

struct ItemModel {
    GLuint vao = 0;
    GLuint vbo = 0;
    GLuint ebo = 0;
    uint32_t indexCount = 0;
};

class ItemModelGenerator {
public:
    static ItemModel generateBlockCube(uint8_t blockId);
    static void destroyModel(ItemModel& model);
};

extern std::unordered_map<uint8_t, ItemModel> g_itemModels;

void loadItemModels();
void unloadItemModels();