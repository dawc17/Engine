#pragma once
#include "glm/ext/vector_float3.hpp"
#include <cstdint>
#include <memory>
#include <glm/glm.hpp>

class AudioEngine
{
    public:
        AudioEngine();
        ~AudioEngine();

        bool init();
        void shutdown();

        void update(float dt);
        void updateListener(const glm::vec3& position, const glm::vec3& forward, const glm::vec3& up);

        void playFootstep(uint8_t blockId, const glm::vec3& worldPos);
        void playBlockBreak(uint8_t blockId, const glm::vec3& worldPos);
        void playBlockPlace(uint8_t blockId, const glm::vec3& worldPos);

        void setWindLoop(bool active);
        void setUnderwaterLoop(bool active);
    
    private:
        struct Impl;
        std::unique_ptr<Impl> impl;
};