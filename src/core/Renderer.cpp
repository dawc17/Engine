#include "Renderer.h"
#include "MainGlobals.h"
#include "../ui/DebugUI.h"
#include "../utils/BlockTypes.h"
#include "../utils/CoordUtils.h"
#include "../rendering/Meshing.h"
#include "../rendering/ItemModelGenerator.h"
#include "../rendering/ToolModelGenerator.h"
#include "../thirdparty/stb_image.h"
#include "../world/Chunk.h"
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <iostream>
#include <vector>
#include <array>
#include <algorithm>

void Renderer::init()
{
    shaderProgram = std::make_unique<Shader>("default.vert", "default.frag");
    shaderProgram->Activate();
    glUniform1i(glGetUniformLocation(shaderProgram->ID, "textureArray"), 0);
    transformLoc  = glGetUniformLocation(shaderProgram->ID, "transform");
    modelLoc      = glGetUniformLocation(shaderProgram->ID, "model");
    timeOfDayLoc  = glGetUniformLocation(shaderProgram->ID, "timeOfDay");
    cameraPosLoc  = glGetUniformLocation(shaderProgram->ID, "cameraPos");
    skyColorLoc   = glGetUniformLocation(shaderProgram->ID, "skyColor");
    fogColorLoc   = glGetUniformLocation(shaderProgram->ID, "fogColor");
    fogDensityLoc = glGetUniformLocation(shaderProgram->ID, "fogDensity");
    ambientLightLoc = glGetUniformLocation(shaderProgram->ID, "ambientLight");

    stbi_set_flip_vertically_on_load(false);

    glGenTextures(1, &textureArray);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D_ARRAY, textureArray);

    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    GLfloat maxAnisotropy = 0.0f;
    glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY, &maxAnisotropy);
    glTexParameterf(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAX_ANISOTROPY, maxAnisotropy);

    int width = 0, height = 0, nrChannels = 0;
    std::string texturePath = resolveTexturePath("assets/textures/blocks.png");
    unsigned char* data = stbi_load(texturePath.c_str(), &width, &height, &nrChannels, 0);

    const int TILE_SIZE = 16;
    const int TILES_X   = 32;
    const int TILES_Y   = 32;
    const int NUM_TILES  = TILES_X * TILES_Y;

    if (data)
    {
        GLenum internalFormat = (nrChannels == 4) ? GL_RGBA8 : GL_RGB8;
        GLenum format = (nrChannels == 4) ? GL_RGBA : GL_RGB;

        glTexImage3D(GL_TEXTURE_2D_ARRAY, 0, internalFormat,
                     TILE_SIZE, TILE_SIZE, NUM_TILES, 0,
                     format, GL_UNSIGNED_BYTE, nullptr);

        std::vector<unsigned char> tileData(TILE_SIZE * TILE_SIZE * nrChannels);
        int tileSizeBytes = TILE_SIZE * nrChannels;
        int atlasRowBytes = width * nrChannels;

        for (int ty = 0; ty < TILES_Y; ty++)
        {
            for (int tx = 0; tx < TILES_X; tx++)
            {
                int tileIndex = ty * TILES_X + tx;
                unsigned char* tileStart = data + (ty * TILE_SIZE) * atlasRowBytes + tx * tileSizeBytes;
                for (int row = 0; row < TILE_SIZE; row++)
                {
                    std::copy(tileStart + row * atlasRowBytes,
                              tileStart + row * atlasRowBytes + tileSizeBytes,
                              tileData.begin() + row * tileSizeBytes);
                }
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
        unsigned char fallback[] = {255, 0, 255, 255};
        glTexImage3D(GL_TEXTURE_2D_ARRAY, 0, GL_RGBA8, 1, 1, 1, 0,
                     GL_RGBA, GL_UNSIGNED_BYTE, fallback);
    }
    stbi_image_free(data);

    initBlockTypes();

    std::string hudIconPath = resolveTexturePath("assets/textures/hud_blocks");
    loadBlockIcons(hudIconPath);
    loadItemModels();
    loadToolModels();

    for (int i = 0; i < 10; ++i)
    {
        std::string destroyPath = resolveTexturePath(
            "assets/textures/destroy/destroy_stage_" + std::to_string(i) + ".png");
        destroyTextures[i] = loadHUDIcon(destroyPath, true);
    }

    selectionShader = std::make_unique<Shader>("selection.vert", "selection.frag");
    selectionTransformLoc = glGetUniformLocation(selectionShader->ID, "transform");
    selectionColorLoc     = glGetUniformLocation(selectionShader->ID, "color");

    destroyShader = std::make_unique<Shader>("destroy.vert", "destroy.frag");
    destroyShader->Activate();
    glUniform1i(glGetUniformLocation(destroyShader->ID, "crackTex"), 0);
    destroyTransformLoc    = glGetUniformLocation(destroyShader->ID, "transform");
    destroyTimeOfDayLoc    = glGetUniformLocation(destroyShader->ID, "timeOfDay");
    destroyAmbientLightLoc = glGetUniformLocation(destroyShader->ID, "ambientLight");
    destroySkyLightLoc     = glGetUniformLocation(destroyShader->ID, "SkyLight");
    destroyFaceShadeLoc    = glGetUniformLocation(destroyShader->ID, "FaceShade");

    itemModelShader = std::make_unique<Shader>("item_model.vert", "item_model.frag");
    itemModelShader->Activate();
    glUniform1i(glGetUniformLocation(itemModelShader->ID, "textureArray"), 0);
    itemTransformLoc    = glGetUniformLocation(itemModelShader->ID, "transform");
    itemTimeOfDayLoc    = glGetUniformLocation(itemModelShader->ID, "timeOfDay");
    itemAmbientLightLoc = glGetUniformLocation(itemModelShader->ID, "ambientLight");

    toolModelShader = std::make_unique<Shader>("tool_model.vert", "tool_model.frag");
    toolModelShader->Activate();
    toolTransformLoc    = glGetUniformLocation(toolModelShader->ID, "transform");
    toolTimeOfDayLoc    = glGetUniformLocation(toolModelShader->ID, "timeOfDay");
    toolAmbientLightLoc = glGetUniformLocation(toolModelShader->ID, "ambientLight");

    waterShader = std::make_unique<Shader>("water.vert", "water.frag");
    waterShader->Activate();
    glUniform1i(glGetUniformLocation(waterShader->ID, "textureArray"), 0);
    waterTransformLoc      = glGetUniformLocation(waterShader->ID, "transform");
    waterModelLoc          = glGetUniformLocation(waterShader->ID, "model");
    waterTimeLoc           = glGetUniformLocation(waterShader->ID, "time");
    waterTimeOfDayLoc      = glGetUniformLocation(waterShader->ID, "timeOfDay");
    waterCameraPosLoc      = glGetUniformLocation(waterShader->ID, "cameraPos");
    waterSkyColorLoc       = glGetUniformLocation(waterShader->ID, "skyColor");
    waterFogColorLoc       = glGetUniformLocation(waterShader->ID, "fogColor");
    waterFogDensityLoc     = glGetUniformLocation(waterShader->ID, "fogDensity");
    waterAmbientLightLoc   = glGetUniformLocation(waterShader->ID, "ambientLight");
    waterEnableCausticsLoc = glGetUniformLocation(waterShader->ID, "enableCaustics");

    const float s = 1.002f;
    const float o = -0.001f;
    float cubeVertices[] = {
        o, o, s+o,  s+o, o, s+o,  s+o, s+o, s+o,  o, s+o, s+o,
        o, o, o,  s+o, o, o,  s+o, s+o, o,  o, s+o, o,
    };
    unsigned int cubeIndices[] = {
        0, 1, 1, 2, 2, 3, 3, 0,
        4, 5, 5, 6, 6, 7, 7, 4,
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
}

void Renderer::cleanup()
{
    glDeleteVertexArrays(1, &selectionVAO);
    glDeleteBuffers(1, &selectionVBO);
    glDeleteBuffers(1, &selectionEBO);
    glDeleteVertexArrays(1, &faceVAO);
    glDeleteBuffers(1, &faceVBO);
    glDeleteBuffers(1, &faceEBO);

    if (selectionShader) selectionShader->Delete();
    if (destroyShader) destroyShader->Delete();
    if (waterShader) waterShader->Delete();
    if (itemModelShader) itemModelShader->Delete();
    if (toolModelShader) toolModelShader->Delete();

    for (GLuint tex : destroyTextures)
    {
        if (tex != 0)
            glDeleteTextures(1, &tex);
    }
    unloadBlockIcons();
    unloadItemModels();
    unloadToolModels();

    if (shaderProgram) shaderProgram->Delete();
}

void Renderer::beginFrame(const FrameParams& fp)
{
    shaderProgram->Activate();
    glBindTexture(GL_TEXTURE_2D_ARRAY, textureArray);

    if (wireframeMode)
        glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
    else
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

    glm::mat4 model(1.0f);
    glm::mat4 mvp = fp.proj * fp.view * model;
    glUniformMatrix4fv(transformLoc, 1, GL_FALSE, glm::value_ptr(mvp));
    glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(model));
    glUniform1f(timeOfDayLoc, fp.sunBrightness);
    glUniform3fv(cameraPosLoc, 1, glm::value_ptr(fp.eyePos));
    glUniform3fv(skyColorLoc, 1, glm::value_ptr(fp.clearCol));
    glUniform3fv(fogColorLoc, 1, glm::value_ptr(fp.fogCol));
    glUniform1f(fogDensityLoc, fp.effectiveFogDensity);
    glUniform1f(ambientLightLoc, fp.ambientLight);
}

void Renderer::renderChunks(const FrameParams& fp, ChunkManager& cm)
{
    for (auto& pair : cm.chunks)
    {
        Chunk* chunk = pair.second.get();
        if (chunk->indexCount > 0)
        {
            glm::mat4 chunkModel = glm::translate(glm::mat4(1.0f),
                glm::vec3(chunk->position.x * CHUNK_SIZE,
                           chunk->position.y * CHUNK_SIZE,
                           chunk->position.z * CHUNK_SIZE));
            glm::mat4 chunkMVP = fp.proj * fp.view * chunkModel;
            glUniformMatrix4fv(transformLoc, 1, GL_FALSE, glm::value_ptr(chunkMVP));
            glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(chunkModel));

            glBindVertexArray(chunk->vao);
            glDrawElements(GL_TRIANGLES, chunk->indexCount, GL_UNSIGNED_INT, 0);
        }
    }
}

void Renderer::renderWater(const FrameParams& fp, ChunkManager& cm)
{
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDepthMask(GL_FALSE);

    waterShader->Activate();
    glBindTexture(GL_TEXTURE_2D_ARRAY, textureArray);

    glUniform1f(waterTimeLoc, fp.gameTime);
    glUniform1f(waterTimeOfDayLoc, fp.sunBrightness);
    glUniform3fv(waterCameraPosLoc, 1, glm::value_ptr(fp.eyePos));
    glUniform3fv(waterSkyColorLoc, 1, glm::value_ptr(fp.skyColor));
    glUniform3fv(waterFogColorLoc, 1, glm::value_ptr(fp.fogCol));
    glUniform1f(waterFogDensityLoc, fp.effectiveFogDensity);
    glUniform1f(waterAmbientLightLoc, fp.ambientLight);
    glUniform1i(waterEnableCausticsLoc, enableCaustics ? 1 : 0);

    for (auto& pair : cm.chunks)
    {
        Chunk* chunk = pair.second.get();
        if (chunk->waterIndexCount > 0)
        {
            glm::mat4 chunkModel = glm::translate(glm::mat4(1.0f),
                glm::vec3(chunk->position.x * CHUNK_SIZE,
                           chunk->position.y * CHUNK_SIZE,
                           chunk->position.z * CHUNK_SIZE));
            glm::mat4 chunkMVP = fp.proj * fp.view * chunkModel;
            glUniformMatrix4fv(waterTransformLoc, 1, GL_FALSE, glm::value_ptr(chunkMVP));
            glUniformMatrix4fv(waterModelLoc, 1, GL_FALSE, glm::value_ptr(chunkModel));

            glBindVertexArray(chunk->waterVao);
            glDrawElements(GL_TRIANGLES, chunk->waterIndexCount, GL_UNSIGNED_INT, 0);
        }
    }

    glDepthMask(GL_TRUE);
    glBindTexture(GL_TEXTURE_2D_ARRAY, textureArray);
}

void Renderer::renderParticles(ParticleSystem& ps, const FrameParams& fp)
{
    ps.render(fp.view, fp.proj, fp.eyePos, fp.sunBrightness, fp.ambientLight);
    glDisable(GL_BLEND);
}

void Renderer::renderSelectionBox(const FrameParams& fp, const std::optional<RaycastHit>& sel)
{
    if (!sel.has_value())
        return;

    glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
    glLineWidth(2.0f);
    glEnable(GL_POLYGON_OFFSET_LINE);
    glPolygonOffset(-1.0f, -1.0f);

    selectionShader->Activate();
    glm::mat4 selectionModel = glm::translate(glm::mat4(1.0f),
        glm::vec3(sel->blockPos));
    glm::mat4 selectionMVP = fp.proj * fp.view * selectionModel;
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

void Renderer::renderDestroyOverlay(const FrameParams& fp, const Player& player,
                                    ChunkManager& cm)
{
    if (!player.isBreaking)
        return;

    int stage = static_cast<int>(player.breakProgress * 10.0f);
    if (stage < 0) stage = 0;
    if (stage > 9) stage = 9;
    GLuint crackTex = destroyTextures[stage];

    if (crackTex == 0)
        return;

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDepthMask(GL_FALSE);

    destroyShader->Activate();
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, crackTex);

    glUniform1f(destroyTimeOfDayLoc, fp.sunBrightness);
    glUniform1f(destroyAmbientLightLoc, fp.ambientLight);

    float skyLightVal = 1.0f;
    {
        glm::ivec3 cpos = worldToChunk(player.breakingBlockPos.x,
                                        player.breakingBlockPos.y,
                                        player.breakingBlockPos.z);
        Chunk* c = cm.getChunk(cpos.x, cpos.y, cpos.z);
        if (c)
        {
            glm::ivec3 local = worldToLocal(player.breakingBlockPos.x,
                                             player.breakingBlockPos.y,
                                             player.breakingBlockPos.z);
            skyLightVal = static_cast<float>(c->skyLight[blockIndex(local.x, local.y, local.z)])
                        / static_cast<float>(MAX_SKY_LIGHT);
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
        glm::mat4 mvpFace = fp.proj * fp.view * ft;
        glUniformMatrix4fv(destroyTransformLoc, 1, GL_FALSE, glm::value_ptr(mvpFace));
        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
    }
    glBindVertexArray(0);

    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
}

void Renderer::renderHeldItem(const FrameParams& fp, const Player& player)
{
    if (inventoryOpen)
        return;

    const ItemStack& held = player.inventory.selectedItem();
    if (held.isEmpty())
        return;

    if (isToolItem(held.blockId))
    {
        auto it = g_toolModels.find(held.blockId);
        if (it == g_toolModels.end() || it->second.indexCount == 0)
            return;

        glClear(GL_DEPTH_BUFFER_BIT);
        if (wireframeMode)
            glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
        else
            glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

        toolModelShader->Activate();
        glUniform1f(toolTimeOfDayLoc, fp.sunBrightness);
        glUniform1f(toolAmbientLightLoc, fp.ambientLight);

        glm::mat4 handProj = glm::perspective(
            glm::radians(70.0f), fp.aspect, 0.01f, 10.0f);

        ToolTransform& tt = g_toolTransform;
        glm::mat4 handModel = glm::mat4(1.0f);
        handModel = glm::translate(handModel,
            glm::vec3(tt.posX, tt.posY, tt.posZ));
        handModel = glm::rotate(handModel,
            glm::radians(tt.rotZ), glm::vec3(0.0f, 0.0f, 1.0f));
        handModel = glm::rotate(handModel,
            glm::radians(tt.rotY), glm::vec3(0.0f, 1.0f, 0.0f));
        handModel = glm::rotate(handModel,
            glm::radians(tt.rotX), glm::vec3(1.0f, 0.0f, 0.0f));
        handModel = glm::scale(handModel, glm::vec3(tt.scale));
        handModel = glm::translate(handModel,
            glm::vec3(-0.5f, -0.5f, -0.03125f));

        glm::mat4 handMVP = handProj * handModel;
        glUniformMatrix4fv(toolTransformLoc, 1, GL_FALSE,
            glm::value_ptr(handMVP));

        glBindVertexArray(it->second.vao);
        glDrawElements(GL_TRIANGLES, it->second.indexCount,
            GL_UNSIGNED_INT, 0);
        glBindVertexArray(0);
    }
    else
    {
        auto it = g_itemModels.find(held.blockId);
        if (it == g_itemModels.end() || it->second.indexCount == 0)
            return;

        glClear(GL_DEPTH_BUFFER_BIT);
        if (wireframeMode)
            glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
        else
            glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

        itemModelShader->Activate();
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D_ARRAY, textureArray);
        glUniform1f(itemTimeOfDayLoc, fp.sunBrightness);
        glUniform1f(itemAmbientLightLoc, fp.ambientLight);

        glm::mat4 handProj = glm::perspective(
            glm::radians(70.0f), fp.aspect, 0.01f, 10.0f);

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
