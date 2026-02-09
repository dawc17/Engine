#pragma once

#include <glad/glad.h>
#include <glm/glm.hpp>
#include <memory>
#include <optional>
#include "../rendering/opengl/ShaderClass.h"
#include "../rendering/ParticleSystem.h"
#include "../gameplay/Player.h"
#include "../world/ChunkManager.h"
#include "../gameplay/Raycast.h"

struct FrameParams
{
    glm::mat4 view;
    glm::mat4 proj;
    glm::vec3 eyePos;
    glm::vec3 skyColor;
    glm::vec3 fogCol;
    glm::vec3 clearCol;
    float aspect;
    float sunBrightness;
    float ambientLight;
    float effectiveFogDensity;
    float gameTime;
};

struct Renderer
{
    std::unique_ptr<Shader> shaderProgram;
    GLint transformLoc = 0, modelLoc = 0, timeOfDayLoc = 0;
    GLint cameraPosLoc = 0, skyColorLoc = 0, fogColorLoc = 0;
    GLint fogDensityLoc = 0, ambientLightLoc = 0;

    unsigned int textureArray = 0;

    std::unique_ptr<Shader> selectionShader;
    GLint selectionTransformLoc = 0, selectionColorLoc = 0;

    std::unique_ptr<Shader> destroyShader;
    GLint destroyTransformLoc = 0, destroyTimeOfDayLoc = 0;
    GLint destroyAmbientLightLoc = 0, destroySkyLightLoc = 0;
    GLint destroyFaceShadeLoc = 0;

    std::unique_ptr<Shader> itemModelShader;
    GLint itemTransformLoc = 0, itemTimeOfDayLoc = 0, itemAmbientLightLoc = 0;

    std::unique_ptr<Shader> toolModelShader;
    GLint toolTransformLoc = 0, toolTimeOfDayLoc = 0, toolAmbientLightLoc = 0;

    std::unique_ptr<Shader> waterShader;
    GLint waterTransformLoc = 0, waterModelLoc = 0, waterTimeLoc = 0;
    GLint waterTimeOfDayLoc = 0, waterCameraPosLoc = 0;
    GLint waterSkyColorLoc = 0, waterFogColorLoc = 0;
    GLint waterFogDensityLoc = 0, waterAmbientLightLoc = 0;
    GLint waterEnableCausticsLoc = 0;

    GLuint destroyTextures[10] = {};

    GLuint selectionVAO = 0, selectionVBO = 0, selectionEBO = 0;
    GLuint faceVAO = 0, faceVBO = 0, faceEBO = 0;

    void init();
    void cleanup();

    void beginFrame(const FrameParams& fp);
    void renderChunks(const FrameParams& fp, ChunkManager& cm);
    void renderWater(const FrameParams& fp, ChunkManager& cm);
    void renderParticles(ParticleSystem& ps, const FrameParams& fp);
    void renderSelectionBox(const FrameParams& fp, const std::optional<RaycastHit>& sel);
    void renderDestroyOverlay(const FrameParams& fp, const Player& player,
                              ChunkManager& cm);
    void renderHeldItem(const FrameParams& fp, const Player& player);
};
