#pragma once
#include "../rendering/opengl/VAO.h"
#include "../rendering/opengl/VBO.h"
#include "../rendering/opengl/ShaderClass.h"
#include <glm/glm.hpp>
#include <vector>
#include <random>
#include <memory>

struct Particle {
    glm::vec3 position;
    glm::vec3 velocity;
    float lifetime;
    float maxLifetime;
    float size;
    float tileIndex;
    glm::vec4 color;
    float skyLight;
};

class ParticleSystem {
public:
    ParticleSystem();
    ~ParticleSystem();

    void init();
    void update(float dt);
    void render(const glm::mat4& view, const glm::mat4& projection, const glm::vec3& cameraPos, float timeOfDay, float ambientLight);

    void spawnBlockBreakParticles(const glm::vec3& blockCenter, int tileIndex, float skyLight, int count = 12, const glm::vec4& tint = glm::vec4(1.0f));
    void spawnParticle(const glm::vec3& pos, const glm::vec3& vel, float lifetime, float size, float tileIndex, float skyLight, const glm::vec4& color = glm::vec4(1.0f));

    void clear();

private:
    std::vector<Particle> particles;
    std::mt19937 rng;

    VAO vao;
    VBO quadVBO;
    VBO instanceVBO;
    std::unique_ptr<Shader> shader;

    static constexpr size_t MAX_PARTICLES = 10000;
    static constexpr float GRAVITY = -20.0f;

    void setupBuffers();
};