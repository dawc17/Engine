#include "AudioEngine.h"
#include "AudioTypes.h"
#include "embedded_assets.h"

#include <algorithm>
#include <cstddef>
#include <cstring>
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

    struct EmbeddedSound
    {
        const unsigned char* data;
        unsigned int size;
    };
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

    std::unordered_map<SurfaceType, std::vector<EmbeddedSound>, SurfaceTypeHash> footstepBySurface;
    std::unordered_map<SurfaceType, std::vector<EmbeddedSound>, SurfaceTypeHash> breakBySurface;
    std::unordered_map<SurfaceType, std::vector<EmbeddedSound>, SurfaceTypeHash> placeBySurface;

    struct ActiveOneShot
    {
        std::unique_ptr<ma_sound> sound;
        std::unique_ptr<ma_decoder> decoder;
        bool initialized = false;

        ActiveOneShot() : sound(std::make_unique<ma_sound>()), decoder(std::make_unique<ma_decoder>()), initialized(false) {
            std::memset(sound.get(), 0, sizeof(ma_sound));
            std::memset(decoder.get(), 0, sizeof(ma_decoder));
        }
    };
    std::vector<ActiveOneShot> activeOneShots;

    ma_sound windLoop{};
    ma_decoder windDecoder{};
    bool windLoopInit = false;

    ma_sound underwaterLoop{};
    ma_decoder underwaterDecoder{};
    bool underwaterLoopInit = false;

    float windTargetVolume = 0.0f;
    float underwaterTargetVolume = 0.0f;

    bool playRandomOneShot(const std::vector<EmbeddedSound>& list,
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
            const EmbeddedSound& embed = list[(startIndex + attempt) % list.size()];

            ActiveOneShot oneShot;

            ma_decoder_config decoderConfig = ma_decoder_config_init_default();
            ma_result result = ma_decoder_init_memory(embed.data, embed.size, &decoderConfig, oneShot.decoder.get());
            if (result != MA_SUCCESS)
            {
                std::cerr << "Audio decoder init failed (code: " << result << ")" << std::endl;
                continue;
            }

            result = ma_sound_init_from_data_source(&engine, oneShot.decoder.get(), 0, nullptr, oneShot.sound.get());
            if (result != MA_SUCCESS)
            {
                ma_decoder_uninit(oneShot.decoder.get());
                std::cerr << "Audio sound init from data source failed (code: " << result << ")" << std::endl;
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
        {embed_footstep_grass1_data, embed_footstep_grass1_size},
        {embed_footstep_grass2_data, embed_footstep_grass2_size},
        {embed_footstep_grass3_data, embed_footstep_grass3_size},
        {embed_footstep_grass4_data, embed_footstep_grass4_size},
        {embed_footstep_grass5_data, embed_footstep_grass5_size},
        {embed_footstep_grass6_data, embed_footstep_grass6_size}
    };
    impl->footstepBySurface[SurfaceType::Stone] = {
        {embed_footstep_stone1_data, embed_footstep_stone1_size},
        {embed_footstep_stone2_data, embed_footstep_stone2_size},
        {embed_footstep_stone3_data, embed_footstep_stone3_size},
        {embed_footstep_stone4_data, embed_footstep_stone4_size},
        {embed_footstep_stone5_data, embed_footstep_stone5_size},
        {embed_footstep_stone6_data, embed_footstep_stone6_size}
    };
    impl->footstepBySurface[SurfaceType::Wood] = {
        {embed_footstep_wood1_data, embed_footstep_wood1_size},
        {embed_footstep_wood2_data, embed_footstep_wood2_size},
        {embed_footstep_wood3_data, embed_footstep_wood3_size},
        {embed_footstep_wood4_data, embed_footstep_wood4_size},
        {embed_footstep_wood5_data, embed_footstep_wood5_size},
        {embed_footstep_wood6_data, embed_footstep_wood6_size}
    };
    impl->footstepBySurface[SurfaceType::Gravel] = {
        {embed_footstep_gravel1_data, embed_footstep_gravel1_size},
        {embed_footstep_gravel2_data, embed_footstep_gravel2_size},
        {embed_footstep_gravel3_data, embed_footstep_gravel3_size},
        {embed_footstep_gravel4_data, embed_footstep_gravel4_size}
    };
    impl->footstepBySurface[SurfaceType::Sand] = {
        {embed_footstep_sand1_data, embed_footstep_sand1_size},
        {embed_footstep_sand2_data, embed_footstep_sand2_size},
        {embed_footstep_sand3_data, embed_footstep_sand3_size},
        {embed_footstep_sand4_data, embed_footstep_sand4_size},
        {embed_footstep_sand5_data, embed_footstep_sand5_size}
    };
    impl->footstepBySurface[SurfaceType::Snow] = {
        {embed_footstep_snow1_data, embed_footstep_snow1_size},
        {embed_footstep_snow2_data, embed_footstep_snow2_size},
        {embed_footstep_snow3_data, embed_footstep_snow3_size},
        {embed_footstep_snow4_data, embed_footstep_snow4_size}
    };
    impl->footstepBySurface[SurfaceType::Cloth] = {
        {embed_footstep_cloth1_data, embed_footstep_cloth1_size},
        {embed_footstep_cloth2_data, embed_footstep_cloth2_size},
        {embed_footstep_cloth3_data, embed_footstep_cloth3_size},
        {embed_footstep_cloth4_data, embed_footstep_cloth4_size}
    };
    impl->footstepBySurface[SurfaceType::Ladder] = {
        {embed_footstep_ladder1_data, embed_footstep_ladder1_size},
        {embed_footstep_ladder2_data, embed_footstep_ladder2_size},
        {embed_footstep_ladder3_data, embed_footstep_ladder3_size},
        {embed_footstep_ladder4_data, embed_footstep_ladder4_size},
        {embed_footstep_ladder5_data, embed_footstep_ladder5_size}
    };
    impl->footstepBySurface[SurfaceType::Default] = {
        {embed_footstep_stone1_data, embed_footstep_stone1_size}
    };

    impl->breakBySurface[SurfaceType::Grass] = {
        {embed_dig_grass1_data, embed_dig_grass1_size},
        {embed_dig_grass2_data, embed_dig_grass2_size},
        {embed_dig_grass3_data, embed_dig_grass3_size},
        {embed_dig_grass4_data, embed_dig_grass4_size}
    };
    impl->breakBySurface[SurfaceType::Stone] = {
        {embed_dig_stone1_data, embed_dig_stone1_size},
        {embed_dig_stone2_data, embed_dig_stone2_size},
        {embed_dig_stone3_data, embed_dig_stone3_size},
        {embed_dig_stone4_data, embed_dig_stone4_size}
    };
    impl->breakBySurface[SurfaceType::Wood] = {
        {embed_dig_wood1_data, embed_dig_wood1_size},
        {embed_dig_wood2_data, embed_dig_wood2_size},
        {embed_dig_wood3_data, embed_dig_wood3_size},
        {embed_dig_wood4_data, embed_dig_wood4_size}
    };
    impl->breakBySurface[SurfaceType::Gravel] = {
        {embed_dig_gravel1_data, embed_dig_gravel1_size},
        {embed_dig_gravel2_data, embed_dig_gravel2_size},
        {embed_dig_gravel3_data, embed_dig_gravel3_size},
        {embed_dig_gravel4_data, embed_dig_gravel4_size}
    };
    impl->breakBySurface[SurfaceType::Sand] = {
        {embed_dig_sand1_data, embed_dig_sand1_size},
        {embed_dig_sand2_data, embed_dig_sand2_size},
        {embed_dig_sand3_data, embed_dig_sand3_size},
        {embed_dig_sand4_data, embed_dig_sand4_size}
    };
    impl->breakBySurface[SurfaceType::Snow] = {
        {embed_dig_snow1_data, embed_dig_snow1_size},
        {embed_dig_snow2_data, embed_dig_snow2_size},
        {embed_dig_snow3_data, embed_dig_snow3_size},
        {embed_dig_snow4_data, embed_dig_snow4_size}
    };
    impl->breakBySurface[SurfaceType::Cloth] = {
        {embed_dig_cloth1_data, embed_dig_cloth1_size},
        {embed_dig_cloth2_data, embed_dig_cloth2_size},
        {embed_dig_cloth3_data, embed_dig_cloth3_size},
        {embed_dig_cloth4_data, embed_dig_cloth4_size}
    };
    impl->breakBySurface[SurfaceType::Default] = {
        {embed_dig_stone1_data, embed_dig_stone1_size}
    };

    impl->placeBySurface = impl->breakBySurface;

    {
        ma_decoder_config decoderConfig = ma_decoder_config_init_default();
        ma_result windLoadResult = ma_decoder_init_memory(embed_liquid_water_data, embed_liquid_water_size,
                                    &decoderConfig, &impl->windDecoder);
        if (windLoadResult == MA_SUCCESS)
        {
            windLoadResult = ma_sound_init_from_data_source(&impl->engine, &impl->windDecoder,
                                    MA_SOUND_FLAG_STREAM, nullptr, &impl->windLoop);
        }
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
            std::cerr << "Audio ambient load failed (wind) code=" << windLoadResult << std::endl;
        }
    }

    {
        ma_decoder_config decoderConfig = ma_decoder_config_init_default();
        ma_result underwaterLoadResult = ma_decoder_init_memory(embed_liquid_swim1_data, embed_liquid_swim1_size,
                                    &decoderConfig, &impl->underwaterDecoder);
        if (underwaterLoadResult == MA_SUCCESS)
        {
            underwaterLoadResult = ma_sound_init_from_data_source(&impl->engine, &impl->underwaterDecoder,
                                    MA_SOUND_FLAG_STREAM, nullptr, &impl->underwaterLoop);
        }
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
            std::cerr << "Audio ambient load failed (underwater) code=" << underwaterLoadResult << std::endl;
        }
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
        {
            ma_sound_uninit(oneShot.sound.get());
            ma_decoder_uninit(oneShot.decoder.get());
        }
    }
    impl->activeOneShots.clear();

    if (impl->windLoopInit)
    {
        ma_sound_uninit(&impl->windLoop);
        ma_decoder_uninit(&impl->windDecoder);
        impl->windLoopInit = false;
    }

    if (impl->underwaterLoopInit)
    {
        ma_sound_uninit(&impl->underwaterLoop);
        ma_decoder_uninit(&impl->underwaterDecoder);
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
            ma_decoder_uninit(impl->activeOneShots[i].decoder.get());
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

static const std::vector<EmbeddedSound>& getSurfaceList(
    const std::unordered_map<SurfaceType, std::vector<EmbeddedSound>, SurfaceTypeHash>& bank,
    SurfaceType surface)
{
    auto it = bank.find(surface);
    if (it != bank.end() && !it->second.empty())
        return it->second;

    static const std::vector<EmbeddedSound> empty;
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

void AudioEngine::setWaterAmbience(float volume)
{
    impl->windTargetVolume = std::clamp(volume, 0.0f, 1.0f);
}

void AudioEngine::setUnderwaterLoop(bool active)
{
    impl->underwaterTargetVolume = active ? 0.25f : 0.0f;
}