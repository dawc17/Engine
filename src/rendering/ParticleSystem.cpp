#include "ParticleSystem.h"
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <algorithm>
#include <random>

ParticleSystem::ParticleSystem()
{
    std::random_device rd;
    rng.seed(rd());
}

ParticleSystem::~ParticleSystem()
{
    vao.Delete();
    quadVBO.Delete();
    instanceVBO.Delete();
    if (shader)
        shader->Delete();
}

void ParticleSystem::init()
{
    shader = std::make_unique<Shader>("particle.vert", "particle.frag");
    setupBuffers();
    particles.reserve(MAX_PARTICLES);
}

void ParticleSystem::setupBuffers()
{
    float quadVertices[] = {
        -0.5f, -0.5f,
         0.5f, -0.5f,
         0.5f,  0.5f,
        -0.5f, -0.5f,
         0.5f,  0.5f,
        -0.5f,  0.5f
    };

    vao.Bind();

    quadVBO = VBO(quadVertices, sizeof(quadVertices), GL_STATIC_DRAW);
    vao.LinkAttrib(quadVBO, 0, 2, GL_FLOAT, 2 * sizeof(float), (void*)0);

    instanceVBO.Init(MAX_PARTICLES * sizeof(float) * 11, GL_DYNAMIC_DRAW);

    GLsizei stride = 11 * sizeof(float);
    vao.LinkAttribInstanced(instanceVBO, 1, 3, GL_FLOAT, stride, (void*)0);
    vao.LinkAttribInstanced(instanceVBO, 2, 1, GL_FLOAT, stride, (void*)(3 * sizeof(float)));
    vao.LinkAttribInstanced(instanceVBO, 3, 1, GL_FLOAT, stride, (void*)(4 * sizeof(float)));
    vao.LinkAttribInstanced(instanceVBO, 4, 4, GL_FLOAT, stride, (void*)(5 * sizeof(float)));
    vao.LinkAttribInstanced(instanceVBO, 5, 1, GL_FLOAT, stride, (void*)(9 * sizeof(float)));
    vao.LinkAttribInstanced(instanceVBO, 6, 1, GL_FLOAT, stride, (void*)(10 * sizeof(float)));

    vao.Unbind();
}

void ParticleSystem::update(float dt)
{
    for (auto& p : particles)
    {
        p.velocity.y += GRAVITY * dt;
        p.position += p.velocity * dt;
        p.lifetime -= dt;
    }

    particles.erase(
        std::remove_if(particles.begin(), particles.end(),
            [](const Particle& p) { return p.lifetime <= 0.0f; }),
        particles.end()
    );
}

void ParticleSystem::render(const glm::mat4& view, const glm::mat4& projection, const glm::vec3& cameraPos, float timeOfDay, float ambientLight)
{
    if (particles.empty())
        return;

    glm::vec3 cameraRight = glm::vec3(view[0][0], view[1][0], view[2][0]);
    glm::vec3 cameraUp = glm::vec3(view[0][1], view[1][1], view[2][1]);

    std::vector<float> instanceData;
    instanceData.reserve(particles.size() * 11);

    for (const auto& p : particles)
    {
        instanceData.push_back(p.position.x);
        instanceData.push_back(p.position.y);
        instanceData.push_back(p.position.z);
        instanceData.push_back(p.size);
        instanceData.push_back(p.tileIndex);
        instanceData.push_back(p.color.r);
        instanceData.push_back(p.color.g);
        instanceData.push_back(p.color.b);
        instanceData.push_back(p.color.a);
        instanceData.push_back(p.lifetime / p.maxLifetime);
        instanceData.push_back(p.skyLight);
    }

    instanceVBO.Update(instanceData.data(), instanceData.size() * sizeof(float));

    shader->Activate();
    glUniformMatrix4fv(glGetUniformLocation(shader->ID, "view"), 1, GL_FALSE, glm::value_ptr(view));
    glUniformMatrix4fv(glGetUniformLocation(shader->ID, "projection"), 1, GL_FALSE, glm::value_ptr(projection));
    glUniform3fv(glGetUniformLocation(shader->ID, "cameraRight"), 1, glm::value_ptr(cameraRight));
    glUniform3fv(glGetUniformLocation(shader->ID, "cameraUp"), 1, glm::value_ptr(cameraUp));
    glUniform1i(glGetUniformLocation(shader->ID, "textureArray"), 0);
    glUniform1f(glGetUniformLocation(shader->ID, "timeOfDay"), timeOfDay);
    glUniform1f(glGetUniformLocation(shader->ID, "ambientLight"), ambientLight);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDepthMask(GL_FALSE);

    vao.Bind();
    glDrawArraysInstanced(GL_TRIANGLES, 0, 6, static_cast<GLsizei>(particles.size()));

    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
    vao.Unbind();
}

void ParticleSystem::spawnBlockBreakParticles(const glm::vec3& blockCenter, int tileIndex, float skyLight, int count)
{
    std::uniform_real_distribution<float> posDist(-0.3f, 0.3f);
    std::uniform_real_distribution<float> velXZ(-3.0f, 3.0f);
    std::uniform_real_distribution<float> velY(2.0f, 6.0f);
    std::uniform_real_distribution<float> sizeDist(0.1f, 0.25f);
    std::uniform_real_distribution<float> lifeDist(0.5f, 1.2f);

    for (int i = 0; i < count && particles.size() < MAX_PARTICLES; ++i)
    {
        Particle p;
        p.position = blockCenter + glm::vec3(posDist(rng), posDist(rng), posDist(rng));
        p.velocity = glm::vec3(velXZ(rng), velY(rng), velXZ(rng));
        p.lifetime = lifeDist(rng);
        p.maxLifetime = p.lifetime;
        p.size = sizeDist(rng);
        p.tileIndex = static_cast<float>(tileIndex);
        p.color = glm::vec4(1.0f);
        p.skyLight = skyLight;
        particles.push_back(p);
    }
}

void ParticleSystem::spawnParticle(const glm::vec3& pos, const glm::vec3& vel, float life, float size, float tileIndex, float skyLight, const glm::vec4& color)
{
    if (particles.size() >= MAX_PARTICLES)
        return;

    Particle p;
    p.position = pos;
    p.velocity = vel;
    p.lifetime = life;
    p.maxLifetime = life;
    p.size = size;
    p.tileIndex = tileIndex;
    p.color = color;
    p.skyLight = skyLight;
    particles.push_back(p);
}

void ParticleSystem::clear()
{
    particles.clear();
}