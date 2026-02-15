#include "AudioEngine.h"
#include "AudioTypes.h"

#include <algorithm>
#include <cstddef>
#include <cstring>
#include <filesystem>
#include <iostream>
#include <memory>
#include <random>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace 
{
    struct SurfaceTypeHash
    {
        size_t operator()(SurfaceType s) const noexcept
        {
            return static_cast<size_t>(s);
        }
    };

    static float lerp(float a, float b, float t)
    {
        return a + (b - a) * t;
    }

    static std::string resolveAudioPath(const std::string& relativePath)
    {
        namespace fs = std::filesystem;

        const fs::path direct(relativePath);
        if (fs::exists(direct))
            return direct.generic_string();

        static const char* prefixes[] = {
            "../",
            "../../",
            "../../../",
            "../../../../"
        };

        for (const char* prefix : prefixes)
        {
            fs::path candidate = fs::path(prefix) / direct;
            if (fs::exists(candidate))
                return candidate.generic_string();
        }

        return relativePath;
    }
}

#define STB_VORBIS_HEADER_ONLY
#include "../thirdparty/stb_vorbis.c"

#define MINIAUDIO_IMPLEMENTATION
#include "../thirdparty/miniaudio.h"

struct AudioEngine::Impl
{
    ma_engine engine{};
    bool initialized = false;

    std::mt19937 rng{std::random_device{}()};

    std::unordered_map<SurfaceType, std::vector<std::string>, SurfaceTypeHash> footstepBySurface;
    std::unordered_map<SurfaceType, std::vector<std::string>, SurfaceTypeHash> breakBySurface;
    std::unordered_map<SurfaceType, std::vector<std::string>, SurfaceTypeHash> placeBySurface;

    struct ActiveOneShot
    {
        std::unique_ptr<ma_sound> sound;
        bool initialized = false;

        ActiveOneShot() : sound(std::make_unique<ma_sound>()), initialized(false) {
            std::memset(sound.get(), 0, sizeof(ma_sound));
        }
    };
    std::vector<ActiveOneShot> activeOneShots;

    ma_sound windLoop{};
    bool windLoopInit = false;

    ma_sound underwaterLoop{};
    bool underwaterLoopInit = false;

    float windTargetVolume = 0.0f;
    float underwaterTargetVolume = 0.0f;
    std::unordered_set<std::string> loggedLoadFailures;

    bool playRandomOneShot(const std::vector<std::string>& list,
                           const glm::vec3& worldPos,
                           float volume,
                           float minPitch,
                           float maxPitch)
    {
        if (!initialized || list.empty())
            return false;

        std::uniform_int_distribution<size_t> startPick(0, list.size() - 1);
        std::uniform_real_distribution<float> pitchRand(minPitch, maxPitch);
        const size_t startIndex = startPick(rng);

        for (size_t attempt = 0; attempt < list.size(); ++attempt)
        {
            const std::string& path = list[(startIndex + attempt) % list.size()];
            const std::string resolved = resolveAudioPath(path);

            ActiveOneShot oneShot;
            ma_result result = ma_sound_init_from_file(&engine, resolved.c_str(), 0, nullptr, nullptr, oneShot.sound.get());
            if (result != MA_SUCCESS)
            {
                if (loggedLoadFailures.insert(path).second)
                {
                    std::cerr << "Audio one-shot load failed: " << path
                              << " (resolved: " << resolved << ", code: " << result << ")" << std::endl;
                }
                continue;
            }

            oneShot.initialized = true;

            ma_sound_set_spatialization_enabled(oneShot.sound.get(), MA_TRUE);
            ma_sound_set_position(oneShot.sound.get(), worldPos.x, worldPos.y, worldPos.z);
            ma_sound_set_volume(oneShot.sound.get(), volume);
            ma_sound_set_pitch(oneShot.sound.get(), pitchRand(rng));
            ma_sound_set_rolloff(oneShot.sound.get(), 1.0f);
            ma_sound_set_min_distance(oneShot.sound.get(), 1.5f);
            ma_sound_set_max_distance(oneShot.sound.get(), 28.0f);

            ma_sound_start(oneShot.sound.get());
            activeOneShots.push_back(std::move(oneShot));
            return true;
        }

        return false;
    }
};

AudioEngine::AudioEngine() : impl(std::make_unique<Impl>())
{
}

AudioEngine::~AudioEngine()
{
    shutdown();
}

bool AudioEngine::init()
{
    if (impl->initialized)
        return true;

    ma_engine_config config = ma_engine_config_init();
    config.listenerCount = 1;

    if (ma_engine_init(&config, &impl->engine) != MA_SUCCESS)
        return false;

    impl->initialized = true;

    impl->footstepBySurface[SurfaceType::Grass] = {
        "assets/sounds/footsteps/grass/grass1.ogg",
        "assets/sounds/footsteps/grass/grass2.ogg",
        "assets/sounds/footsteps/grass/grass3.ogg",
        "assets/sounds/footsteps/grass/grass4.ogg",
        "assets/sounds/footsteps/grass/grass5.ogg",
        "assets/sounds/footsteps/grass/grass6.ogg"
    };
    impl->footstepBySurface[SurfaceType::Stone] = {
        "assets/sounds/footsteps/stone/stone1.ogg",
        "assets/sounds/footsteps/stone/stone2.ogg",
        "assets/sounds/footsteps/stone/stone3.ogg",
        "assets/sounds/footsteps/stone/stone4.ogg",
        "assets/sounds/footsteps/stone/stone5.ogg",
        "assets/sounds/footsteps/stone/stone6.ogg"
    };
    impl->footstepBySurface[SurfaceType::Wood] = {
        "assets/sounds/footsteps/wood/wood1.ogg",
        "assets/sounds/footsteps/wood/wood2.ogg",
        "assets/sounds/footsteps/wood/wood3.ogg",
        "assets/sounds/footsteps/wood/wood4.ogg",
        "assets/sounds/footsteps/wood/wood5.ogg",
        "assets/sounds/footsteps/wood/wood6.ogg"
    };
    impl->footstepBySurface[SurfaceType::Gravel] = {
        "assets/sounds/footsteps/gravel/gravel1.ogg",
        "assets/sounds/footsteps/gravel/gravel2.ogg",
        "assets/sounds/footsteps/gravel/gravel3.ogg",
        "assets/sounds/footsteps/gravel/gravel4.ogg"
    };
    impl->footstepBySurface[SurfaceType::Sand] = {
        "assets/sounds/footsteps/sand/sand1.ogg",
        "assets/sounds/footsteps/sand/sand2.ogg",
        "assets/sounds/footsteps/sand/sand3.ogg",
        "assets/sounds/footsteps/sand/sand4.ogg",
        "assets/sounds/footsteps/sand/sand5.ogg"
    };
    impl->footstepBySurface[SurfaceType::Snow] = {
        "assets/sounds/footsteps/snow/snow1.ogg",
        "assets/sounds/footsteps/snow/snow2.ogg",
        "assets/sounds/footsteps/snow/snow3.ogg",
        "assets/sounds/footsteps/snow/snow4.ogg"
    };
    impl->footstepBySurface[SurfaceType::Cloth] = {
        "assets/sounds/footsteps/cloth/cloth1.ogg",
        "assets/sounds/footsteps/cloth/cloth2.ogg",
        "assets/sounds/footsteps/cloth/cloth3.ogg",
        "assets/sounds/footsteps/cloth/cloth4.ogg"
    };
    impl->footstepBySurface[SurfaceType::Ladder] = {
        "assets/sounds/footsteps/ladder/ladder1.ogg",
        "assets/sounds/footsteps/ladder/ladder2.ogg",
        "assets/sounds/footsteps/ladder/ladder3.ogg",
        "assets/sounds/footsteps/ladder/ladder4.ogg",
        "assets/sounds/footsteps/ladder/ladder5.ogg"
    };
    impl->footstepBySurface[SurfaceType::Default] = {
        "assets/sounds/footsteps/stone/stone1.ogg"
    };

    impl->breakBySurface[SurfaceType::Grass] = {
        "assets/sounds/block_dig/grass/grass1.ogg",
        "assets/sounds/block_dig/grass/grass2.ogg",
        "assets/sounds/block_dig/grass/grass3.ogg",
        "assets/sounds/block_dig/grass/grass4.ogg"
    };
    impl->breakBySurface[SurfaceType::Stone] = {
        "assets/sounds/block_dig/stone/stone1.ogg",
        "assets/sounds/block_dig/stone/stone2.ogg",
        "assets/sounds/block_dig/stone/stone3.ogg",
        "assets/sounds/block_dig/stone/stone4.ogg"
    };
    impl->breakBySurface[SurfaceType::Wood] = {
        "assets/sounds/block_dig/wood/wood1.ogg",
        "assets/sounds/block_dig/wood/wood2.ogg",
        "assets/sounds/block_dig/wood/wood3.ogg",
        "assets/sounds/block_dig/wood/wood4.ogg"
    };
    impl->breakBySurface[SurfaceType::Gravel] = {
        "assets/sounds/block_dig/gravel/gravel1.ogg",
        "assets/sounds/block_dig/gravel/gravel2.ogg",
        "assets/sounds/block_dig/gravel/gravel3.ogg",
        "assets/sounds/block_dig/gravel/gravel4.ogg"
    };
    impl->breakBySurface[SurfaceType::Sand] = {
        "assets/sounds/block_dig/sand/sand1.ogg",
        "assets/sounds/block_dig/sand/sand2.ogg",
        "assets/sounds/block_dig/sand/sand3.ogg",
        "assets/sounds/block_dig/sand/sand4.ogg"
    };
    impl->breakBySurface[SurfaceType::Snow] = {
        "assets/sounds/block_dig/snow/snow1.ogg",
        "assets/sounds/block_dig/snow/snow2.ogg",
        "assets/sounds/block_dig/snow/snow3.ogg",
        "assets/sounds/block_dig/snow/snow4.ogg"
    };
    impl->breakBySurface[SurfaceType::Cloth] = {
        "assets/sounds/block_dig/cloth/cloth1.ogg",
        "assets/sounds/block_dig/cloth/cloth2.ogg",
        "assets/sounds/block_dig/cloth/cloth3.ogg",
        "assets/sounds/block_dig/cloth/cloth4.ogg"
    };
    impl->breakBySurface[SurfaceType::Default] = {
        "assets/sounds/block_dig/stone/stone1.ogg"
    };

    impl->placeBySurface = impl->breakBySurface;

    const std::string windPath = resolveAudioPath("assets/sounds/liquid/water.ogg");
    ma_result windLoadResult = ma_sound_init_from_file(&impl->engine, windPath.c_str(),
                                MA_SOUND_FLAG_STREAM, nullptr, nullptr, &impl->windLoop);
    if (windLoadResult == MA_SUCCESS)
    {
        impl->windLoopInit = true;
        ma_sound_set_looping(&impl->windLoop, MA_TRUE);
        ma_sound_set_spatialization_enabled(&impl->windLoop, MA_FALSE);
        ma_sound_set_volume(&impl->windLoop, 0.0f);
        ma_sound_start(&impl->windLoop);
    }
    else
    {
        std::cerr << "Audio ambient load failed (wind): " << windPath
                  << " code=" << windLoadResult << std::endl;
    }

    const std::string underwaterPath = resolveAudioPath("assets/sounds/liquid/swim1.ogg");
    ma_result underwaterLoadResult = ma_sound_init_from_file(&impl->engine, underwaterPath.c_str(),
                                MA_SOUND_FLAG_STREAM, nullptr, nullptr, &impl->underwaterLoop);
    if (underwaterLoadResult == MA_SUCCESS)
    {
        impl->underwaterLoopInit = true;
        ma_sound_set_looping(&impl->underwaterLoop, MA_TRUE);
        ma_sound_set_spatialization_enabled(&impl->underwaterLoop, MA_FALSE);
        ma_sound_set_volume(&impl->underwaterLoop, 0.0f);
        ma_sound_start(&impl->underwaterLoop);
    }
    else
    {
        std::cerr << "Audio ambient load failed (underwater): " << underwaterPath
                  << " code=" << underwaterLoadResult << std::endl;
    }

    return true;
}

void AudioEngine::shutdown()
{
    if (!impl || !impl->initialized)
        return;

    for (auto& oneShot : impl->activeOneShots)
    {
        if (oneShot.initialized)
            ma_sound_uninit(oneShot.sound.get());
    }
    impl->activeOneShots.clear();

    if (impl->windLoopInit)
    {
        ma_sound_uninit(&impl->windLoop);
        impl->windLoopInit = false;
    }

    if (impl->underwaterLoopInit)
    {
        ma_sound_uninit(&impl->underwaterLoop);
        impl->underwaterLoopInit = false;
    }

    ma_engine_uninit(&impl->engine);
    impl->initialized = false;
}

void AudioEngine::update(float dt)
{
    if (!impl->initialized)
        return;

    for (size_t i = 0; i < impl->activeOneShots.size();)
    {
        if (impl->activeOneShots[i].initialized && ma_sound_at_end(impl->activeOneShots[i].sound.get()))
        {
            ma_sound_uninit(impl->activeOneShots[i].sound.get());
            impl->activeOneShots.erase(impl->activeOneShots.begin() + static_cast<long long>(i));
            continue;
        }
        ++i;
    }

    const float fadeSpeed = 6.0f;
    const float alpha = std::clamp(dt * fadeSpeed, 0.0f, 1.0f);

    if (impl->windLoopInit)
    {
        float current = ma_sound_get_volume(&impl->windLoop);
        ma_sound_set_volume(&impl->windLoop, lerp(current, impl->windTargetVolume, alpha));
    }

    if (impl->underwaterLoopInit)
    {
        float current = ma_sound_get_volume(&impl->underwaterLoop);
        ma_sound_set_volume(&impl->underwaterLoop, lerp(current, impl->underwaterTargetVolume, alpha));
    }
}

void AudioEngine::updateListener(const glm::vec3 &position, const glm::vec3 &forward, const glm::vec3 &up)
{
    if (!impl->initialized)
        return;

    ma_engine_listener_set_position(&impl->engine, 0, position.x, position.y, position.z);
    ma_engine_listener_set_direction(&impl->engine, 0, forward.x, forward.y, forward.z);
    ma_engine_listener_set_world_up(&impl->engine, 0, up.x, up.y, up.z);
}

static const std::vector<std::string>& getSurfaceList(
    const std::unordered_map<SurfaceType, std::vector<std::string>, SurfaceTypeHash>& bank,
    SurfaceType surface)
{
    auto it = bank.find(surface);
    if (it != bank.end() && !it->second.empty())
        return it->second;

    static const std::vector<std::string> empty;
    auto fallback = bank.find(SurfaceType::Default);
    if (fallback != bank.end())
        return fallback->second;

    return empty;
}

void AudioEngine::playFootstep(uint8_t blockId, const glm::vec3 &worldPos)
{
    if (!impl->initialized)
        return;

    SurfaceType surface = blockToSurface(blockId);
    const auto& list = getSurfaceList(impl->footstepBySurface, surface);
    if (!impl->playRandomOneShot(list, worldPos, 0.35f, 0.95f, 1.05f) && surface != SurfaceType::Default)
    {
        const auto& fallback = getSurfaceList(impl->footstepBySurface, SurfaceType::Default);
        impl->playRandomOneShot(fallback, worldPos, 0.35f, 0.95f, 1.05f);
    }
}

void AudioEngine::playBlockBreak(uint8_t blockId, const glm::vec3 &worldPos)
{
    if (!impl->initialized)
        return;

    SurfaceType surface = blockToSurface(blockId);
    const auto& list = getSurfaceList(impl->breakBySurface, surface);
    if (!impl->playRandomOneShot(list, worldPos, 0.55f, 0.95f, 1.05f) && surface != SurfaceType::Default)
    {
        const auto& fallback = getSurfaceList(impl->breakBySurface, SurfaceType::Default);
        impl->playRandomOneShot(fallback, worldPos, 0.55f, 0.95f, 1.05f);
    }
}

void AudioEngine::playBlockPlace(uint8_t blockId, const glm::vec3 &worldPos)
{
    if (!impl->initialized)
        return;

    SurfaceType surface = blockToSurface(blockId);
    const auto& list = getSurfaceList(impl->placeBySurface, surface);
    if (!impl->playRandomOneShot(list, worldPos, 0.45f, 0.98f, 1.02f) && surface != SurfaceType::Default)
    {
        const auto& fallback = getSurfaceList(impl->placeBySurface, SurfaceType::Default);
        impl->playRandomOneShot(fallback, worldPos, 0.45f, 0.98f, 1.02f);
    }
}

void AudioEngine::setWindLoop(bool active)
{
    impl->windTargetVolume = active ? 0.08f : 0.0f;
}

void AudioEngine::setUnderwaterLoop(bool active)
{
    impl->underwaterTargetVolume = active ? 0.25f : 0.0f;
}