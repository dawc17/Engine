#include "ParticleSystem.h"
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <algorithm>
#include <iostream>
#include <ostream>
#include <random>

ParticleSystem::ParticleSystem()
    : vao(0), quadVBO(0), instanceVBO(0), shaderProgram(0)
{
    std::random_device rd;
    rng.seed(rd());
}

ParticleSystem::~ParticleSystem() {
    if (vao) glDeleteVertexArrays(1, &vao);
    if (quadVBO) glDeleteBuffers(1, &quadVBO);
    if (instanceVBO) glDeleteBuffers(1, &instanceVBO);
    if (shaderProgram) glDeleteProgram(shaderProgram);
}

void ParticleSystem::init() {
    compileShaders();
    setupBuffers();
    particles.reserve(MAX_PARTICLES);
}

void ParticleSystem::compileShaders() {
    const char* vertexSource = R"(
    #version 460 core
    layout (location = 0) in vec2 aQuadPos;
    layout (location = 1) in vec3 aInstancePos;
    layout (location = 2) in float aSize;
    layout (location = 3) in float aTileIndex;
    layout (location = 4) in vec4 aColor;
    layout (location = 5) in float aLife;

    out vec2 TexCoord;
    flat out float TileIndex;
    out vec4 ParticleColor;
    out float Lifetime;

    uniform mat4 view;
    uniform mat4 projection;
    uniform vec3 cameraRight;
    uniform vec3 cameraUp;

    void main() {
        vec3 vertexPos = aInstancePos
            + cameraRight * aQuadPos.x * aSize
            + cameraUp * aQuadPos.y * aSize;

        gl_Position = projection * view * vec4(vertexPos, 1.0);
        TexCoord = aQuadPos + 0.5;
        TileIndex = aTileIndex;
        ParticleColor = aColor;
        Lifetime = aLife;
    }
    )";

    const char* fragmentSource = R"(
        #version 460 core
        in vec2 TexCoord;
        flat in float TileIndex;
        in vec4 ParticleColor;
        in float Lifetime;

        out vec4 FragColor;

        uniform sampler2DArray textureArray;

        void main() {
            vec4 texColor = texture(textureArray, vec3(TexCoord, TileIndex));
            if (texColor.a < 0.1)
                discard;
            
                float fade = smoothstep(0.0, 0.3, Lifetime);
                FragColor = texColor * ParticleColor;
                FragColor.a *= fade;
        }
    )";

    GLuint vertexShader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertexShader, 1, &vertexSource, nullptr);
    glCompileShader(vertexShader);

    GLint success;
    glGetShaderiv(vertexShader, GL_COMPILE_STATUS, &success);
    if (!success) {
        char infoLog[512];
        glGetShaderInfoLog(vertexShader, 512, nullptr, infoLog);
        std::cerr << "Particle vertex shader errror: " << infoLog << std::endl;
    }

    GLuint fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragmentShader, 1, &fragmentSource, nullptr);
    glCompileShader(fragmentShader);

    glGetShaderiv(fragmentShader, GL_COMPILE_STATUS, &success);
    if (!success) {
        char infoLog[512];
        glGetShaderInfoLog(fragmentShader, 512, nullptr, infoLog);
        std::cerr << "Particle fragment shader error: " << infoLog << std::endl;
    }

    shaderProgram = glCreateProgram();
    glAttachShader(shaderProgram, vertexShader);
    glAttachShader(shaderProgram, fragmentShader);
    glLinkProgram(shaderProgram);

    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);
}

void ParticleSystem::setupBuffers() {
    float quadVertices[] = {
        -0.5f, -0.5f,
        0.5f, -0.5f,
        0.5f, 0.5f,
        -0.5f, -0.5f,
        0.5f, 0.5f,
        -0.5f, 0.5f
    };

    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &quadVBO);
    glGenBuffers(1, &instanceVBO);

    glBindVertexArray(vao);

    glBindBuffer(GL_ARRAY_BUFFER, quadVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertices), quadVertices, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);

    glBindBuffer(GL_ARRAY_BUFFER, instanceVBO);
    glBufferData(GL_ARRAY_BUFFER, MAX_PARTICLES * sizeof(float) * 10, nullptr, GL_DYNAMIC_DRAW);

    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 10 * sizeof(float), (void*)0);
    glVertexAttribDivisor(1, 1);

    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 1, GL_FLOAT, GL_FALSE, 10 * sizeof(float), (void*)(3 * sizeof(float)));
    glVertexAttribDivisor(2, 1);

    glEnableVertexAttribArray(3);
    glVertexAttribPointer(3, 1, GL_FLOAT, GL_FALSE, 10 * sizeof(float), (void*)(4 * sizeof(float)));
    glVertexAttribDivisor(3, 1);

    glEnableVertexAttribArray(4);
    glVertexAttribPointer(4, 4, GL_FLOAT, GL_FALSE, 10 * sizeof(float), (void*)(5 * sizeof(float)));
    glVertexAttribDivisor(4, 1);

    glEnableVertexAttribArray(5);
    glVertexAttribPointer(5, 1, GL_FLOAT, GL_FALSE, 10 * sizeof(float), (void*)(9 * sizeof(float)));
    glVertexAttribDivisor(5, 1);

    glBindVertexArray(0);
}

void ParticleSystem::update(float dt) {
    for (auto& p : particles) {
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

void ParticleSystem::render(const glm::mat4& view, const glm::mat4& projection, const glm::vec3& cameraPos) {
    if (particles.empty())
        return;

    glm::vec3 cameraRight = glm::vec3(view[0][0], view[1][0], view[2][0]);
    glm::vec3 cameraUp = glm::vec3(view[0][1], view[1][1], view[2][1]);

    std::vector<float> instanceData;
    instanceData.reserve(particles.size() * 10);

    for (const auto& p : particles) {
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
    }
    
    glBindBuffer(GL_ARRAY_BUFFER, instanceVBO);
    glBufferSubData(GL_ARRAY_BUFFER, 0, instanceData.size() * sizeof(float), instanceData.data());

    glUseProgram(shaderProgram);
    glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "view"), 1, GL_FALSE, glm::value_ptr(view));
    glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "projection"), 1, GL_FALSE, glm::value_ptr(projection));
    glUniform3fv(glGetUniformLocation(shaderProgram, "cameraRight"), 1, glm::value_ptr(cameraRight));
    glUniform3fv(glGetUniformLocation(shaderProgram, "cameraUp"), 1, glm::value_ptr(cameraUp));
    glUniform1i(glGetUniformLocation(shaderProgram, "textureArray"), 0);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDepthMask(GL_FALSE);

    glBindVertexArray(vao);
    glDrawArraysInstanced(GL_TRIANGLES, 0, 6, static_cast<GLsizei>(particles.size()));

    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
    glBindVertexArray(0);
}

void ParticleSystem::spawnBlockBreakParticles(const glm::vec3& blockCenter, int tileIndex, int count)
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
        particles.push_back(p);
    }
}

void ParticleSystem::spawnParticle(const glm::vec3& pos, const glm::vec3& vel, float life, float size, float tileIndex, const glm::vec4& color)
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
    particles.push_back(p);
}

void ParticleSystem::clear()
{
    particles.clear();
}