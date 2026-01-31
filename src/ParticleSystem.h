#pragma once
#include "VAO.h"
#include "VBO.h"
#include "ShaderClass.h"
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
};

class ParticleSystem {
public:
    ParticleSystem();
    ~ParticleSystem();

    void init();
    void update(float dt);
    void render(const glm::mat4& view, const glm::mat4& projection, const glm::vec3& cameraPos);

    void spawnBlockBreakParticles(const glm::vec3& blockCenter, int tileIndex, int count = 12);
    void spawnParticle(const glm::vec3& pos, const glm::vec3& vel, float lifetime, float size, float tileIndex, const glm::vec4& color = glm::vec4(1.0f));

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