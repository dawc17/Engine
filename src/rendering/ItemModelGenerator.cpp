#include "ItemModelGenerator.h"
#include "../utils/BlockTypes.h"
#include "Meshing.h"
#include <iostream>
#include <cstddef>
#include <vector>

std::unordered_map<uint8_t, ItemModel> g_itemModels;

ItemModel ItemModelGenerator::generateBlockCube(uint8_t blockId)
{
    const BlockType& block = g_blockTypes[blockId];

    static const glm::ivec3 DIRS[6] = {
        {1,0,0}, {-1,0,0}, {0,1,0}, {0,-1,0}, {0,0,1}, {0,0,-1}
    };

    static const float FACE_UV[4][2] = {
        {1, 0}, {1, 1}, {0, 1}, {0, 0}
    };

    std::vector<ItemVertex> vertices;
    std::vector<uint32_t> indices;

    for (int dir = 0; dir < 6; dir++)
    {
        glm::ivec3 n = DIRS[dir];
        int axis = 0;
        if (n.y != 0) axis = 1;
        if (n.z != 0) axis = 2;
        int u = (axis + 1) % 3;
        int v = (axis + 2) % 3;
        int axisOffset = (n[axis] > 0) ? 1 : 0;

        float tileIndex = static_cast<float>(block.faceTexture[dir]);
        int rotation = block.faceRotation[dir];
        float faceShade = FACE_SHADE[dir];

        uint32_t base = static_cast<uint32_t>(vertices.size());

        for (int vi = 0; vi < 4; vi++)
        {
            float faceU = FACE_UV[vi][0];
            float faceV = FACE_UV[vi][1];

            glm::vec3 pos(0.0f);
            pos[axis] = static_cast<float>(axisOffset);
            pos[u] = (faceU > 0.5f) ? 1.0f : 0.0f;
            pos[v] = (faceV > 0.5f) ? 1.0f : 0.0f;

            float localU = (faceU > 0.5f) ? 1.0f : 0.0f;
            float localV = (faceV > 0.5f) ? 1.0f : 0.0f;

            switch (rotation)
            {
                case 1:
                {
                    float tmp = localU;
                    localU = localV;
                    localV = 1.0f - tmp;
                    break;
                }
                case 2:
                    localV = 1.0f - localV;
                    break;
                case 3:
                {
                    float tmp = localU;
                    localU = 1.0f - localV;
                    localV = tmp;
                    break;
                }
                default:
                    break;
            }

            vertices.push_back({pos, {localU, localV}, tileIndex, faceShade});
        }

        indices.push_back(base);
        indices.push_back(base + 1);
        indices.push_back(base + 2);
        indices.push_back(base);
        indices.push_back(base + 2);
        indices.push_back(base + 3);
    }

    ItemModel model;

    glGenVertexArrays(1, &model.vao);
    glGenBuffers(1, &model.vbo);
    glGenBuffers(1, &model.ebo);

    glBindVertexArray(model.vao);

    glBindBuffer(GL_ARRAY_BUFFER, model.vbo);
    glBufferData(GL_ARRAY_BUFFER,
                 vertices.size() * sizeof(ItemVertex),
                 vertices.data(), GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, model.ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER,
                 indices.size() * sizeof(uint32_t),
                 indices.data(), GL_STATIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(ItemVertex),
                          (void*)0);
    glEnableVertexAttribArray(0);

    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(ItemVertex),
                          (void*)offsetof(ItemVertex, uv));
    glEnableVertexAttribArray(1);

    glVertexAttribPointer(2, 1, GL_FLOAT, GL_FALSE, sizeof(ItemVertex),
                          (void*)offsetof(ItemVertex, tileIndex));
    glEnableVertexAttribArray(2);

    glVertexAttribPointer(3, 1, GL_FLOAT, GL_FALSE, sizeof(ItemVertex),
                          (void*)offsetof(ItemVertex, faceShade));
    glEnableVertexAttribArray(3);

    glBindVertexArray(0);

    model.indexCount = static_cast<uint32_t>(indices.size());
    return model;
}

void ItemModelGenerator::destroyModel(ItemModel& model)
{
    if (model.vao) glDeleteVertexArrays(1, &model.vao);
    if (model.vbo) glDeleteBuffers(1, &model.vbo);
    if (model.ebo) glDeleteBuffers(1, &model.ebo);
    model = {};
}

void loadItemModels()
{
    uint8_t blockIds[] = {1, 2, 3, 4, 5, 6, 7, 8};
    for (uint8_t blockId : blockIds)
    {
        ItemModel model = ItemModelGenerator::generateBlockCube(blockId);
        if (model.indexCount > 0)
        {
            g_itemModels[blockId] = model;
            std::cout << "Generated block model for block "
                      << (int)blockId << std::endl;
        }
    }
}

void unloadItemModels()
{
    for (auto& [id, model] : g_itemModels)
        ItemModelGenerator::destroyModel(model);
    g_itemModels.clear();
}