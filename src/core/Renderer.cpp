#include "Renderer.h"
#include "MainGlobals.h"
#include "../ui/DebugUI.h"
#include "../utils/BlockTypes.h"
#include "../utils/CoordUtils.h"
#include "../rendering/Meshing.h"
#include "../rendering/ItemModelGenerator.h"
#include "../rendering/ToolModelGenerator.h"
#include "../rendering/Frustum.h"
#include "../thirdparty/stb_image.h"
#include "../world/Chunk.h"
#include "glm/ext/matrix_transform.hpp"
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <iostream>
#include <array>
#include "embedded_assets.h"

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

    constexpr int TILE_SIZE = 16;
    constexpr int NUM_LAYERS = TEX_COUNT;

    glTexImage3D(GL_TEXTURE_2D_ARRAY, 0, GL_RGBA8,
                 TILE_SIZE, TILE_SIZE, NUM_LAYERS, 0,
                 GL_RGBA, GL_UNSIGNED_BYTE, nullptr);

    struct TexEntry { int layer; const unsigned char* data; unsigned int size; };
    TexEntry entries[] = {
        { TEX_DIRT,              embed_dirt_png_data,              embed_dirt_png_size },
        { TEX_GRASS_TOP,         embed_grass_top_png_data,         embed_grass_top_png_size },
        { TEX_GRASS_SIDE_SNOWED, embed_grass_side_snowed_png_data, embed_grass_side_snowed_png_size },
        { TEX_STONE,             embed_stone_png_data,             embed_stone_png_size },
        { TEX_SAND,              embed_sand_png_data,              embed_sand_png_size },
        { TEX_LOG_OAK,           embed_log_oak_png_data,           embed_log_oak_png_size },
        { TEX_LOG_OAK_TOP,       embed_log_oak_top_png_data,       embed_log_oak_top_png_size },
        { TEX_LEAVES_OAK,        embed_leaves_oak_png_data,        embed_leaves_oak_png_size },
        { TEX_GLASS,             embed_glass_png_data,             embed_glass_png_size },
        { TEX_PLANKS_OAK,        embed_planks_oak_png_data,        embed_planks_oak_png_size },
        { TEX_COBBLESTONE,       embed_cobblestone_png_data,       embed_cobblestone_png_size },
        { TEX_LOG_SPRUCE,        embed_log_spruce_png_data,        embed_log_spruce_png_size },
        { TEX_LOG_SPRUCE_TOP,    embed_log_spruce_top_png_data,    embed_log_spruce_top_png_size },
        { TEX_LEAVES_SPRUCE,     embed_leaves_spruce_png_data,     embed_leaves_spruce_png_size },
        { TEX_PLANKS_SPRUCE,     embed_planks_spruce_png_data,     embed_planks_spruce_png_size },
        { TEX_SNOW,              embed_snow_png_data,              embed_snow_png_size },
    };

    for (const auto& e : entries)
    {
        int w = 0, h = 0, ch = 0;
        unsigned char* pixels = stbi_load_from_memory(e.data, static_cast<int>(e.size), &w, &h, &ch, 4);
        if (pixels)
        {
            glTexSubImage3D(GL_TEXTURE_2D_ARRAY, 0,
                            0, 0, e.layer,
                            TILE_SIZE, TILE_SIZE, 1,
                            GL_RGBA, GL_UNSIGNED_BYTE, pixels);
            stbi_image_free(pixels);
        }
        else
        {
            unsigned char fallback[TILE_SIZE * TILE_SIZE * 4];
            for (int p = 0; p < TILE_SIZE * TILE_SIZE; p++)
            {
                fallback[p * 4 + 0] = 255;
                fallback[p * 4 + 1] = 0;
                fallback[p * 4 + 2] = 255;
                fallback[p * 4 + 3] = 255;
            }
            glTexSubImage3D(GL_TEXTURE_2D_ARRAY, 0,
                            0, 0, e.layer,
                            TILE_SIZE, TILE_SIZE, 1,
                            GL_RGBA, GL_UNSIGNED_BYTE, fallback);
        }
    }

    {
        int dw = 0, dh = 0, dch = 0, ow = 0, oh = 0, och = 0;
        unsigned char* dirtPx = stbi_load_from_memory(embed_dirt_png_data, static_cast<int>(embed_dirt_png_size), &dw, &dh, &dch, 4);
        unsigned char* overlayPx = stbi_load_from_memory(embed_grass_side_overlay_png_data, static_cast<int>(embed_grass_side_overlay_png_size), &ow, &oh, &och, 4);

        unsigned char composite[TILE_SIZE * TILE_SIZE * 4];
        if (dirtPx && overlayPx)
        {
            for (int p = 0; p < TILE_SIZE * TILE_SIZE; p++)
            {
                unsigned char oa = overlayPx[p * 4 + 3];
                if (oa > 0)
                {
                    float a = static_cast<float>(oa) / 255.0f;
                    for (int c = 0; c < 3; c++)
                    {
                        float bg = static_cast<float>(dirtPx[p * 4 + c]) / 255.0f;
                        float fg = static_cast<float>(overlayPx[p * 4 + c]) / 255.0f;
                        composite[p * 4 + c] = static_cast<unsigned char>(
                            (fg * a + bg * (1.0f - a)) * 255.0f);
                    }
                    composite[p * 4 + 3] = 255;
                }
                else
                {
                    composite[p * 4 + 0] = dirtPx[p * 4 + 0];
                    composite[p * 4 + 1] = dirtPx[p * 4 + 1];
                    composite[p * 4 + 2] = dirtPx[p * 4 + 2];
                    composite[p * 4 + 3] = 254;
                }
            }
        }
        else
        {
            for (int p = 0; p < TILE_SIZE * TILE_SIZE; p++)
            {
                composite[p * 4 + 0] = 255;
                composite[p * 4 + 1] = 0;
                composite[p * 4 + 2] = 255;
                composite[p * 4 + 3] = 255;
            }
        }
        glTexSubImage3D(GL_TEXTURE_2D_ARRAY, 0,
                        0, 0, TEX_GRASS_SIDE,
                        TILE_SIZE, TILE_SIZE, 1,
                        GL_RGBA, GL_UNSIGNED_BYTE, composite);
        if (dirtPx) stbi_image_free(dirtPx);
        if (overlayPx) stbi_image_free(overlayPx);
    }


    glGenerateMipmap(GL_TEXTURE_2D_ARRAY);
    std::cout << "Loaded " << NUM_LAYERS << " block textures" << std::endl;

    initBlockTypes();

    itemModelShader = std::make_unique<Shader>("item_model.vert", "item_model.frag");
    itemModelShader->Activate();
    glUniform1i(glGetUniformLocation(itemModelShader->ID, "textureArray"), 0);
    itemTransformLoc    = glGetUniformLocation(itemModelShader->ID, "transform");
    itemTimeOfDayLoc    = glGetUniformLocation(itemModelShader->ID, "timeOfDay");
    itemAmbientLightLoc = glGetUniformLocation(itemModelShader->ID, "ambientLight");

    loadItemModels();
    generateBlockIcons(textureArray, itemModelShader.get());
    loadToolModels();

    {
        const unsigned char* dData[] = {
            embed_destroy_stage_0_png_data, embed_destroy_stage_1_png_data,
            embed_destroy_stage_2_png_data, embed_destroy_stage_3_png_data,
            embed_destroy_stage_4_png_data, embed_destroy_stage_5_png_data,
            embed_destroy_stage_6_png_data, embed_destroy_stage_7_png_data,
            embed_destroy_stage_8_png_data, embed_destroy_stage_9_png_data,
        };
        const unsigned int dSizes[] = {
            embed_destroy_stage_0_png_size, embed_destroy_stage_1_png_size,
            embed_destroy_stage_2_png_size, embed_destroy_stage_3_png_size,
            embed_destroy_stage_4_png_size, embed_destroy_stage_5_png_size,
            embed_destroy_stage_6_png_size, embed_destroy_stage_7_png_size,
            embed_destroy_stage_8_png_size, embed_destroy_stage_9_png_size,
        };
        for (int i = 0; i < 10; ++i)
            destroyTextures[i] = loadHUDIcon(dData[i], dSizes[i], true);
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
    frustumSolidTested = 0;
    frustumSolidCulled = 0;
    frustumSolidDrawn = 0;
    frustumWaterTested = 0;
    frustumWaterCulled = 0;
    frustumWaterDrawn = 0;

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
    const glm::mat4 viewProj = fp.proj * fp.view;
    const Frustum frustum = Frustum::fromMatrix(viewProj);
    const float chunkSizeF = static_cast<float>(CHUNK_SIZE);

    for (auto& pair : cm.chunks)
    {
        Chunk* chunk = pair.second.get();
        if (chunk->indexCount == 0)
            continue;
        frustumSolidTested++;

        const glm::vec3 chunkMin = glm::vec3(chunk->position) * chunkSizeF;
        const glm::vec3 chunkMax = chunkMin + glm::vec3(chunkSizeF);

        if (!frustum.intersectsAABB(chunkMin, chunkMax))
        {
            frustumSolidCulled++;
            continue;
        }

        glm::mat4 chunkModel = glm::translate(
            glm::mat4(1.0f),
            glm::vec3(chunk->position.x * CHUNK_SIZE,
                      chunk->position.y * CHUNK_SIZE,
                      chunk->position.z * CHUNK_SIZE));
        glm::mat4 chunkMVP = viewProj * chunkModel;

        glUniformMatrix4fv(transformLoc, 1, GL_FALSE, glm::value_ptr(chunkMVP));
        glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(chunkModel));

        glBindVertexArray(chunk->vao);
        glDrawElements(GL_TRIANGLES, chunk->indexCount, GL_UNSIGNED_INT, 0);
        frustumSolidDrawn++;
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

    const glm::mat4 viewProj = fp.proj * fp.view;
    const Frustum frustum = Frustum::fromMatrix(viewProj);
    const float chunkSizeF = static_cast<float>(CHUNK_SIZE);

    for (auto& pair : cm.chunks)
    {
        Chunk* chunk = pair.second.get();
        if (chunk->waterIndexCount == 0)
            continue;
        frustumWaterTested++;

        const glm::vec3 chunkMin = glm::vec3(chunk->position) * chunkSizeF;
        const glm::vec3 chunkMax = chunkMin + glm::vec3(chunkSizeF);

        if (!frustum.intersectsAABB(chunkMin, chunkMax))
        {
            frustumWaterCulled++;
            continue;
        }

        glm::mat4 chunkModel = glm::translate(
            glm::mat4(1.0f),
            glm::vec3(chunk->position.x * CHUNK_SIZE,
                      chunk->position.y * CHUNK_SIZE,
                      chunk->position.z * CHUNK_SIZE));
        glm::mat4 chunkMVP = viewProj * chunkModel;

        glUniformMatrix4fv(waterTransformLoc, 1, GL_FALSE, glm::value_ptr(chunkMVP));
        glUniformMatrix4fv(waterModelLoc, 1, GL_FALSE, glm::value_ptr(chunkModel));

        glBindVertexArray(chunk->waterVao);
        glDrawElements(GL_TRIANGLES, chunk->waterIndexCount, GL_UNSIGNED_INT, 0);
        frustumWaterDrawn++;
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

        constexpr float PI = 3.14159265f;

        float swingT = player.isSwinging ? player.swingProgress : 0.0f;
        float swingWave = std::sin(swingT * PI);

        float placeT = player.isPlacing ? player.placeProgress : 0.0f;
        float placeTri = 1.0f - std::abs(2.0f * placeT - 1.0f);
        float placeEase = placeTri * placeTri * (3.0f - 2.0f * placeTri);

        if (player.isSwinging)
        {
            handModel = glm::translate(handModel, glm::vec3(-0.08f * swingWave, -0.12f * swingWave, 0.06f * swingWave));
            handModel = glm::rotate(handModel, glm::radians(-60.0f * swingWave), glm::vec3(0.0f, 0.0f, 1.0f));
        }
        if (player.isPlacing)
        {
            handModel = glm::translate(handModel, glm::vec3(0.05f * placeEase, -0.02f * placeEase, -0.13f * placeEase));
            handModel = glm::rotate(handModel, glm::radians(18.0f * placeEase), glm::vec3(1.0f, 0.0f, 0.0f));
        }
            
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

        BlockTransform& bt = g_blockTransform;
        glm::mat4 handModel = glm::mat4(1.0f);
        handModel = glm::translate(handModel,
            glm::vec3(bt.posX, bt.posY, bt.posZ));
        handModel = glm::rotate(handModel,
            glm::radians(bt.rotZ), glm::vec3(0.0f, 0.0f, 1.0f));
        handModel = glm::rotate(handModel,
            glm::radians(bt.rotY), glm::vec3(0.0f, 1.0f, 0.0f));
        handModel = glm::rotate(handModel,
            glm::radians(bt.rotX), glm::vec3(1.0f, 0.0f, 0.0f));

        constexpr float PI = 3.14159265f;

        float swingT = player.isSwinging ? player.swingProgress : 0.0f;
        float swingWave = std::sin(swingT * PI);

        float placeT = player.isPlacing ? player.placeProgress : 0.0f;
        float placeTri = 1.0f - std::abs(2.0f * placeT - 1.0f);
        float placeEase = placeTri * placeTri * (3.0f - 2.0f * placeTri);

        if (player.isSwinging)
        {
            handModel = glm::translate(handModel,
                glm::vec3(g_blockSwingTuning.posX * swingWave,
                          g_blockSwingTuning.posY * swingWave,
                          g_blockSwingTuning.posZ * swingWave));
            handModel = glm::rotate(handModel,
                glm::radians(g_blockSwingTuning.rotX * swingWave), glm::vec3(1.0f, 0.0f, 0.0f));
            handModel = glm::rotate(handModel,
                glm::radians(g_blockSwingTuning.rotZ * swingWave), glm::vec3(0.0f, 0.0f, 1.0f));
        }
        if (player.isPlacing)
        {
            handModel = glm::translate(handModel,
                glm::vec3(g_blockPlaceTuning.posX * placeEase,
                          g_blockPlaceTuning.posY * placeEase,
                          g_blockPlaceTuning.posZ * placeEase));
            handModel = glm::rotate(handModel,
                glm::radians(g_blockPlaceTuning.rotX * placeEase), glm::vec3(1.0f, 0.0f, 0.0f));
            handModel = glm::rotate(handModel,
                glm::radians(g_blockPlaceTuning.rotZ * placeEase), glm::vec3(0.0f, 0.0f, 1.0f));
        }

        handModel = glm::scale(handModel, glm::vec3(bt.scale));
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
