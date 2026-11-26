#include "audio/AudioBinding.hpp"

#include <lua.hpp>

#include <cstdint>
#include <cstring>
#include <format>
#include <memory>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <vector>

#include "audio/EffectBinding.hpp"
#include "audio/TapBinding.hpp"
#include "haylen/2d/graphics/Camera.hpp"
#include "haylen/core/Engine.hpp"
#include "haylen/io/MemoryReader.hpp"
#include "haylen/lua/Binding.hpp"
#include "haylen/lua/ClassBuilder.hpp"
#include "haylen/lua/Runtime.hpp"
#include "haylen/lua/Stack.hpp"
#include "haylen/lua/Table.hpp"
#include "haylen/lua/TypeConverter.hpp"
#include "haylen/lua/Userdata.hpp"

namespace haylen::lua::audio {

haylen::audio::Mixer& AudioBinding::getMixer(lua_State* L) {
    return lua::Runtime::getEngine(L).getAudio();
}

// The options of a play are read in one pass over the keys the table holds, because sound effects play often.
haylen::audio::Mixer::PlayOptions AudioBinding::readPlayOptions(lua_State* L, int index, std::span<const std::string_view> fields) {
    haylen::audio::Mixer::PlayOptions options;
    if (lua_isnoneornil(L, index)) {
        return options;
    }

    luaL_checktype(L, index, LUA_TTABLE);

    // A voice becomes positional when the options give it a place in the world.
    std::optional<float> x;
    std::optional<float> y;
    // clang-format off
    lua::Table::readFields(L, index, fields, {}, [L, &options, &x, &y](std::string_view key) {
        if (key == "bus") {
            lua::Table::readValue(L, key, options.bus);
        } else if (key == "volume") {
            lua::Table::readValue(L, key, options.volume);
        } else if (key == "pitch") {
            lua::Table::readValue(L, key, options.pitch);
        } else if (key == "pitchVariation") {
            lua::Table::readValue(L, key, options.pitchVariation);
        } else if (key == "pan") {
            lua::Table::readValue(L, key, options.pan);
        } else if (key == "loop") {
            lua::Table::readValue(L, key, options.loop);
        } else if (key == "fadeIn") {
            lua::Table::readValue(L, key, options.fadeIn);
        } else if (key == "startAt") {
            lua::Table::readValue(L, key, options.startAt);
        } else if (key == "processMode") {
            lua::Table::readValue(L, key, options.processMode);
        } else if (key == "effects") {
            options.effects = readEffects(L);
        } else if (key == "priority") {
            int priority = 0;
            lua::Table::readValue(L, key, priority);
            if (priority < 0 || priority > 255) {
                luaL_error(L, "The priority of a voice runs from 0 to 255.");
            }

            options.priority = static_cast<std::uint8_t>(priority);
        } else if (key == "loopStart") {
            lua::Table::readValue(L, key, options.loopStart);
        } else if (key == "loopEnd") {
            lua::Table::readValue(L, key, options.loopEnd.emplace());
        } else if (key == "x") {
            lua::Table::readValue(L, key, x.emplace());
        } else if (key == "y") {
            lua::Table::readValue(L, key, y.emplace());
        }
    });
    // clang-format on
    if (x || y) {
        options.position = haylen::math::Vec2{x.value_or(0.0F), y.value_or(0.0F)};
    }

    return options;
}

// Reads the list of effects on top of the stack.
std::vector<std::shared_ptr<haylen::audio::Effect>> AudioBinding::readEffects(lua_State* L) {
    std::vector<std::shared_ptr<haylen::audio::Effect>> list;

    if (!lua_istable(L, -1)) {
        luaL_error(L, "The option \"effects\" must be a list of audio effects.");
    }

    const lua_Integer count = luaL_len(L, -1);
    for (lua_Integer position = 1; position <= count; ++position) {
        lua_geti(L, -1, position);
        std::shared_ptr<haylen::audio::Effect> effect = EffectBinding::test(L, -1);
        if (!effect) {
            luaL_error(L, "The option \"effects\" must be a list of audio effects.");
        }

        list.push_back(std::move(effect));
        lua_pop(L, 1);
    }

    return list;
}

void AudioBinding::pushEffects(lua_State* L, const std::vector<std::shared_ptr<haylen::audio::Effect>>& effects) {
    lua_createtable(L, static_cast<int>(effects.size()), 0);
    lua_Integer position = 1;
    for (const std::shared_ptr<haylen::audio::Effect>& effect : effects) {
        EffectBinding::push(L, effect);
        lua_rawseti(L, -2, position++);
    }
}

template <typename Sample> std::vector<Sample> AudioBinding::readSamples(std::string_view bytes) {
    if (bytes.size() % sizeof(Sample) != 0) {
        throw std::invalid_argument(std::format("Samples of this format take {} bytes each, which {} bytes do not fill.", sizeof(Sample), bytes.size()));
    }

    std::vector<Sample> samples(bytes.size() / sizeof(Sample));
    std::memcpy(samples.data(), bytes.data(), bytes.size());
    return samples;
}

// Makes a sound from bytes with `newSound(bytes, options)`, a string or a data view: a WAV, Ogg Vorbis, MP3 or FLAC file, decoded unless `{stream = true}`, or raw samples with `{format = 'float32' or 'int16', sampleRate, channels}`.
int AudioBinding::newSound(lua_State* L) {
    const std::string_view bytes = lua::TypeConverter::readBytes(L, 1);
    bool stream = false;
    std::string format;
    std::uint32_t sampleRate = 0;
    std::uint32_t channels = 0;
    if (!lua_isnoneornil(L, 2)) {
        luaL_checktype(L, 2, LUA_TTABLE);
        lua::Table::checkFields(L, 2, {kSoundFields});
        lua::Table::readField(L, 2, "stream", stream);
        lua::Table::readField(L, 2, "format", format);
        lua::Table::readField(L, 2, "sampleRate", sampleRate);
        lua::Table::readField(L, 2, "channels", channels);
    }

    const std::span<const std::uint8_t> encoded(reinterpret_cast<const std::uint8_t*>(bytes.data()), bytes.size());
    if (format.empty()) {
        if (sampleRate != 0 || channels != 0) {
            throw std::invalid_argument("Only raw samples take a sample rate and channels, together with their format.");
        }

        lua::Stack::push(L, stream ? haylen::audio::Sound::stream(std::make_shared<haylen::io::MemoryReader>(std::make_shared<const std::vector<std::uint8_t>>(encoded.begin(), encoded.end()))) : haylen::audio::Sound::decode(encoded));
        return 1;
    }

    if (stream) {
        throw std::invalid_argument("Raw samples are decoded already, so they cannot stream.");
    }

    if (format == "float32") {
        lua::Stack::push(L, haylen::audio::Sound::fromSamples(std::span<const float>(readSamples<float>(bytes)), channels, sampleRate));
        return 1;
    }

    if (format == "int16") {
        lua::Stack::push(L, haylen::audio::Sound::fromSamples(std::span<const std::int16_t>(readSamples<std::int16_t>(bytes)), channels, sampleRate));
        return 1;
    }

    throw std::invalid_argument("The format of raw samples is \"float32\" or \"int16\", not \"" + format + "\".");
}

int AudioBinding::play(lua_State* L) {
    lua::Stack::push(L, getMixer(L).play(lua::Stack::read<haylen::audio::Sound>(L, 1), readPlayOptions(L, 2, kPlayFields)));
    return 1;
}

int AudioBinding::stop(lua_State* L) {
    getMixer(L).stop(lua::Stack::read<haylen::audio::Mixer::VoiceId>(L, 1), lua::Converter<float>::readOr(L, 2, 0.0));
    return 0;
}

// Limits a sound with `setLimit(sound, {voices = 4, cooldown = 0.03, steal = 'oldest'})`, where no voices and no cooldown remove the limit.
int AudioBinding::setLimit(lua_State* L) {
    const haylen::audio::Sound sound = lua::Stack::read<haylen::audio::Sound>(L, 1);
    luaL_checktype(L, 2, LUA_TTABLE);
    lua::Table::checkFields(L, 2, {kLimitFields});
    haylen::audio::Mixer::Limit limit;
    lua::Table::readField(L, 2, "voices", limit.voices);
    lua::Table::readField(L, 2, "cooldown", limit.cooldown);
    std::string steal;
    lua::Table::readField(L, 2, "steal", steal);
    if (!steal.empty()) {
        const std::optional<haylen::audio::Mixer::Limit::Steal> found = haylen::audio::Mixer::Limit::stealFromName(steal);
        if (!found) {
            return luaL_error(L, "The \"steal\" of a sound limit must be \"oldest\", \"quietest\" or \"none\".");
        }

        limit.steal = *found;
    }

    getMixer(L).setLimit(sound, limit);
    return 0;
}

int AudioBinding::limit(lua_State* L) {
    const std::optional<haylen::audio::Mixer::Limit> found = getMixer(L).getLimit(lua::Stack::read<haylen::audio::Sound>(L, 1));
    if (!found) {
        lua_pushnil(L);
        return 1;
    }

    lua_createtable(L, 0, 3);
    lua_pushinteger(L, static_cast<lua_Integer>(found->voices));
    lua_setfield(L, -2, "voices");
    lua::Stack::push(L, found->cooldown);
    lua_setfield(L, -2, "cooldown");
    lua::Stack::push(L, haylen::audio::Mixer::Limit::stealName(found->steal));
    lua_setfield(L, -2, "steal");
    return 1;
}

int AudioBinding::pause(lua_State* L) {
    getMixer(L).setPaused(lua::Stack::read<haylen::audio::Mixer::VoiceId>(L, 1), true);
    return 0;
}

int AudioBinding::resume(lua_State* L) {
    getMixer(L).setPaused(lua::Stack::read<haylen::audio::Mixer::VoiceId>(L, 1), false);
    return 0;
}

int AudioBinding::paused(lua_State* L) {
    lua::Stack::push(L, getMixer(L).isPaused(lua::Stack::read<haylen::audio::Mixer::VoiceId>(L, 1)));
    return 1;
}

int AudioBinding::setVolume(lua_State* L) {
    getMixer(L).setVolume(lua::Stack::read<haylen::audio::Mixer::VoiceId>(L, 1), lua::Stack::read<float>(L, 2));
    return 0;
}

int AudioBinding::setPitch(lua_State* L) {
    getMixer(L).setPitch(lua::Stack::read<haylen::audio::Mixer::VoiceId>(L, 1), lua::Stack::read<float>(L, 2));
    return 0;
}

int AudioBinding::pitch(lua_State* L) {
    lua::Stack::push(L, getMixer(L).getPitch(lua::Stack::read<haylen::audio::Mixer::VoiceId>(L, 1)));
    return 1;
}

int AudioBinding::seedVariation(lua_State* L) {
    getMixer(L).seedVariation(lua::Stack::read<std::uint64_t>(L, 1));
    return 0;
}

int AudioBinding::setPan(lua_State* L) {
    getMixer(L).setPan(lua::Stack::read<haylen::audio::Mixer::VoiceId>(L, 1), lua::Stack::read<float>(L, 2));
    return 0;
}

int AudioBinding::setPosition(lua_State* L) {
    getMixer(L).setPosition(lua::Stack::read<haylen::audio::Mixer::VoiceId>(L, 1), {lua::Stack::read<float>(L, 2), lua::Stack::read<float>(L, 3)});
    return 0;
}

int AudioBinding::processMode(lua_State* L) {
    lua::Stack::push(L, getMixer(L).getProcessMode(lua::Stack::read<haylen::audio::Mixer::VoiceId>(L, 1)));
    return 1;
}

int AudioBinding::active(lua_State* L) {
    lua::Stack::push(L, getMixer(L).isActive(lua::Stack::read<haylen::audio::Mixer::VoiceId>(L, 1)));
    return 1;
}

int AudioBinding::cursor(lua_State* L) {
    lua::Stack::push(L, getMixer(L).getCursor(lua::Stack::read<haylen::audio::Mixer::VoiceId>(L, 1)));
    return 1;
}

int AudioBinding::setCursor(lua_State* L) {
    getMixer(L).setCursor(lua::Stack::read<haylen::audio::Mixer::VoiceId>(L, 1), lua::Stack::read<float>(L, 2));
    return 0;
}

int AudioBinding::stopAll(lua_State* L) {
    getMixer(L).stopAll(lua::Converter<float>::readOr(L, 1, 0.0));
    return 0;
}

int AudioBinding::voiceCount(lua_State* L) {
    lua::Stack::push(L, getMixer(L).getVoiceCount());
    return 1;
}

int AudioBinding::pauseAll(lua_State* L) {
    getMixer(L).pauseAll();
    return 0;
}

int AudioBinding::resumeAll(lua_State* L) {
    getMixer(L).resumeAll();
    return 0;
}

int AudioBinding::interrupted(lua_State* L) {
    lua::Stack::push(L, getMixer(L).isInterrupted());
    return 1;
}

int AudioBinding::addEffect(lua_State* L) {
    getMixer(L).addEffect(lua::Stack::read<haylen::audio::Mixer::VoiceId>(L, 1), EffectBinding::check(L, 2));
    return 0;
}

int AudioBinding::removeEffect(lua_State* L) {
    getMixer(L).removeEffect(lua::Stack::read<haylen::audio::Mixer::VoiceId>(L, 1), *EffectBinding::check(L, 2));
    return 0;
}

int AudioBinding::effects(lua_State* L) {
    pushEffects(L, getMixer(L).getEffects(lua::Stack::read<haylen::audio::Mixer::VoiceId>(L, 1)));
    return 1;
}

int AudioBinding::playMusic(lua_State* L) {
    haylen::audio::Mixer::MusicOptions options;
    if (!lua_isnoneornil(L, 2)) {
        luaL_checktype(L, 2, LUA_TTABLE);
        lua::Table::checkFields(L, 2, {kMusicFields});
        lua::Table::readField(L, 2, "bus", options.bus);
        lua::Table::readField(L, 2, "volume", options.volume);
        lua::Table::readField(L, 2, "fade", options.fade);
        lua::Table::readField(L, 2, "loop", options.loop);
        lua::Table::readField(L, 2, "startAt", options.startAt);
        lua::Table::readField(L, 2, "loopStart", options.loopStart);
        lua::Table::readField(L, 2, "loopEnd", options.loopEnd);
    }

    lua::Stack::push(L, getMixer(L).playMusic(lua::Stack::read<haylen::audio::Sound>(L, 1), options));
    return 1;
}

int AudioBinding::stopMusic(lua_State* L) {
    getMixer(L).stopMusic(lua::Converter<float>::readOr(L, 1, 1.0));
    return 0;
}

int AudioBinding::music(lua_State* L) {
    const haylen::audio::Sound current = getMixer(L).getMusic();
    if (!current.isValid()) {
        lua_pushnil(L);
        return 1;
    }

    lua::Stack::push(L, current);
    return 1;
}

int AudioBinding::createBus(lua_State* L) {
    getMixer(L).createBus(lua::Stack::read<std::string>(L, 1), lua_isnoneornil(L, 2) ? std::string_view("master") : lua::Stack::read<std::string_view>(L, 2));
    return 0;
}

int AudioBinding::setBusVolume(lua_State* L) {
    getMixer(L).setBusVolume(lua::Stack::read<std::string_view>(L, 1), lua::Stack::read<float>(L, 2), lua::Converter<float>::readOr(L, 3, 0.0));
    return 0;
}

int AudioBinding::busVolume(lua_State* L) {
    lua::Stack::push(L, getMixer(L).getBusVolume(lua::Stack::read<std::string_view>(L, 1)));
    return 1;
}

int AudioBinding::setBusMuted(lua_State* L) {
    getMixer(L).setBusMuted(lua::Stack::read<std::string_view>(L, 1), lua::Stack::read<bool>(L, 2));
    return 0;
}

int AudioBinding::busMuted(lua_State* L) {
    lua::Stack::push(L, getMixer(L).isBusMuted(lua::Stack::read<std::string_view>(L, 1)));
    return 1;
}

int AudioBinding::setBusProcessMode(lua_State* L) {
    getMixer(L).setBusProcessMode(lua::Stack::read<std::string_view>(L, 1), lua::Stack::read<haylen::core::ProcessMode>(L, 2));
    return 0;
}

int AudioBinding::busProcessMode(lua_State* L) {
    lua::Stack::push(L, getMixer(L).getBusProcessMode(lua::Stack::read<std::string_view>(L, 1)));
    return 1;
}

int AudioBinding::addBusEffect(lua_State* L) {
    getMixer(L).addBusEffect(lua::Stack::read<std::string_view>(L, 1), EffectBinding::check(L, 2));
    return 0;
}

int AudioBinding::removeBusEffect(lua_State* L) {
    getMixer(L).removeBusEffect(lua::Stack::read<std::string_view>(L, 1), *EffectBinding::check(L, 2));
    return 0;
}

int AudioBinding::busEffects(lua_State* L) {
    pushEffects(L, getMixer(L).getBusEffects(lua::Stack::read<std::string_view>(L, 1)));
    return 1;
}

int AudioBinding::buses(lua_State* L) {
    lua::Stack::push(L, getMixer(L).getBuses());
    return 1;
}

// Lists every bus in alphabetical order as `{name, voices, playing, paused, processing}`.
int AudioBinding::busStats(lua_State* L) {
    std::vector<haylen::audio::Mixer::BusStats> stats;
    getMixer(L).readBusStats(stats);
    lua_createtable(L, static_cast<int>(stats.size()), 0);
    lua_Integer position = 1;
    for (const haylen::audio::Mixer::BusStats& bus : stats) {
        lua_createtable(L, 0, 5);
        lua::Stack::push(L, bus.name);
        lua_setfield(L, -2, "name");
        lua::Stack::push(L, bus.voices);
        lua_setfield(L, -2, "voices");
        lua::Stack::push(L, bus.playing);
        lua_setfield(L, -2, "playing");
        lua::Stack::push(L, bus.paused);
        lua_setfield(L, -2, "paused");
        lua::Stack::push(L, bus.processing);
        lua_setfield(L, -2, "processing");
        lua_rawseti(L, -2, position++);
    }

    return 1;
}

int AudioBinding::setListener(lua_State* L) {
    getMixer(L).setListener({lua::Stack::read<float>(L, 1), lua::Stack::read<float>(L, 2)});
    return 0;
}

int AudioBinding::listener(lua_State* L) {
    const haylen::math::Vec2 position = getMixer(L).getListener();
    lua::Stack::push(L, position.x);
    lua::Stack::push(L, position.y);
    return 2;
}

// Follows a camera with `followCamera(camera)`, or stops following with `followCamera(nil)`.
int AudioBinding::followCamera(lua_State* L) {
    const haylen::graphics2d::Camera* camera = lua_isnoneornil(L, 1) ? nullptr : &lua::Userdata::check<haylen::graphics2d::Camera>(L, 1);
    lua_settop(L, 1);
    lua_setfield(L, LUA_REGISTRYINDEX, kCameraKey);
    getMixer(L).followCamera(camera);
    return 0;
}

// Changes the fields that `setSpatialization({model, minDistance, maxDistance, rolloff, panDistance, doppler, speedOfSound})` names and keeps the others.
int AudioBinding::setSpatialization(lua_State* L) {
    luaL_checktype(L, 1, LUA_TTABLE);
    lua::Table::checkFields(L, 1, {kSpatializationFields});
    haylen::audio::Mixer& mixer = getMixer(L);
    haylen::audio::Mixer::Spatialization settings = mixer.getSpatialization();
    lua::Table::readField(L, 1, "model", settings.model);
    lua::Table::readField(L, 1, "minDistance", settings.minDistance);
    lua::Table::readField(L, 1, "maxDistance", settings.maxDistance);
    lua::Table::readField(L, 1, "rolloff", settings.rolloff);
    lua::Table::readField(L, 1, "panDistance", settings.panDistance);
    lua::Table::readField(L, 1, "doppler", settings.doppler);
    lua::Table::readField(L, 1, "speedOfSound", settings.speedOfSound);
    mixer.setSpatialization(settings);
    return 0;
}

int AudioBinding::spatialization(lua_State* L) {
    const haylen::audio::Mixer::Spatialization& settings = getMixer(L).getSpatialization();
    lua_createtable(L, 0, 7);
    lua::Stack::push(L, settings.model);
    lua_setfield(L, -2, "model");
    lua::Stack::push(L, settings.minDistance);
    lua_setfield(L, -2, "minDistance");
    lua::Stack::push(L, settings.maxDistance);
    lua_setfield(L, -2, "maxDistance");
    lua::Stack::push(L, settings.rolloff);
    lua_setfield(L, -2, "rolloff");
    lua::Stack::push(L, settings.panDistance);
    lua_setfield(L, -2, "panDistance");
    lua::Stack::push(L, settings.doppler);
    lua_setfield(L, -2, "doppler");
    lua::Stack::push(L, settings.speedOfSound);
    lua_setfield(L, -2, "speedOfSound");
    return 1;
}

int AudioBinding::sampleRate(lua_State* L) {
    lua::Stack::push(L, getMixer(L).getSampleRate());
    return 1;
}

int AudioBinding::channels(lua_State* L) {
    lua::Stack::push(L, getMixer(L).getChannels());
    return 1;
}

int AudioBinding::hasDevice(lua_State* L) {
    lua::Stack::push(L, getMixer(L).hasDevice());
    return 1;
}

int AudioBinding::outputAvailable(lua_State* L) {
    lua::Stack::push(L, getMixer(L).isOutputAvailable());
    return 1;
}

int AudioBinding::outputOpening(lua_State* L) {
    lua::Stack::push(L, getMixer(L).isOutputOpening());
    return 1;
}

int AudioBinding::outputState(lua_State* L) {
    lua::Stack::push(L, haylen::audio::Mixer::outputStateName(getMixer(L).getOutputState()));
    return 1;
}

int AudioBinding::outputLatency(lua_State* L) {
    lua::Stack::push(L, getMixer(L).getOutputLatency());
    return 1;
}

int AudioBinding::outputMuted(lua_State* L) {
    lua::Stack::push(L, getMixer(L).isOutputMuted());
    return 1;
}

int AudioBinding::setBackground(lua_State* L) {
    lua::Runtime::getEngine(L).setBackgroundAudio(lua::Stack::read<bool>(L, 1));
    return 0;
}

int AudioBinding::background(lua_State* L) {
    lua::Stack::push(L, lua::Runtime::getEngine(L).isBackgroundAudio());
    return 1;
}

int AudioBinding::restartOutput(lua_State* L) {
    getMixer(L).restartOutput();
    return 0;
}

int AudioBinding::soundChannels(lua_State* L) {
    lua::Stack::push(L, lua::Userdata::check<haylen::audio::Sound>(L, 1).getChannels());
    return 1;
}

int AudioBinding::soundSampleRate(lua_State* L) {
    lua::Stack::push(L, lua::Userdata::check<haylen::audio::Sound>(L, 1).getSampleRate());
    return 1;
}

int AudioBinding::soundFrameCount(lua_State* L) {
    lua::Stack::push(L, lua::Userdata::check<haylen::audio::Sound>(L, 1).getFrameCount());
    return 1;
}

int AudioBinding::soundStreamed(lua_State* L) {
    lua::Stack::push(L, lua::Userdata::check<haylen::audio::Sound>(L, 1).isStreamed());
    return 1;
}

int AudioBinding::soundDuration(lua_State* L) {
    lua::Stack::push(L, lua::Userdata::check<haylen::audio::Sound>(L, 1).getDuration());
    return 1;
}

int AudioBinding::open(lua_State* L) {
    const luaL_Reg functions[] = {
        {"newSound", &lua::Binding::native<&newSound>}, {"play", &lua::Binding::native<&play>}, {"stop", &lua::Binding::native<&stop>}, {"setLimit", &lua::Binding::native<&setLimit>}, {"limit", &lua::Binding::native<&limit>}, {"pause", &lua::Binding::native<&pause>}, {"resume", &lua::Binding::native<&resume>}, {"paused", &lua::Binding::native<&paused>}, {"setVolume", &lua::Binding::native<&setVolume>}, {"setPitch", &lua::Binding::native<&setPitch>}, {"pitch", &lua::Binding::native<&pitch>}, {"seedVariation", &lua::Binding::native<&seedVariation>}, {"setPan", &lua::Binding::native<&setPan>}, {"setPosition", &lua::Binding::native<&setPosition>}, {"processMode", &lua::Binding::native<&processMode>}, {"active", &lua::Binding::native<&active>}, {"cursor", &lua::Binding::native<&cursor>}, {"setCursor", &lua::Binding::native<&setCursor>}, {"stopAll", &lua::Binding::native<&stopAll>}, {"voiceCount", &voiceCount}, {"pauseAll", &pauseAll}, {"resumeAll", &resumeAll}, {"interrupted", &interrupted}, {"newEffect", &lua::Binding::native<&EffectBinding::newEffect>}, {"addEffect", &lua::Binding::native<&addEffect>}, {"removeEffect", &lua::Binding::native<&removeEffect>}, {"effects", &lua::Binding::native<&effects>}, {"playMusic", &lua::Binding::native<&playMusic>}, {"stopMusic", &lua::Binding::native<&stopMusic>}, {"music", &music}, {"createBus", &lua::Binding::native<&createBus>}, {"setBusVolume", &lua::Binding::native<&setBusVolume>}, {"busVolume", &lua::Binding::native<&busVolume>}, {"setBusMuted", &lua::Binding::native<&setBusMuted>}, {"busMuted", &lua::Binding::native<&busMuted>}, {"setBusProcessMode", &lua::Binding::native<&setBusProcessMode>}, {"busProcessMode", &lua::Binding::native<&busProcessMode>}, {"addBusEffect", &lua::Binding::native<&addBusEffect>}, {"removeBusEffect", &lua::Binding::native<&removeBusEffect>}, {"busEffects", &lua::Binding::native<&busEffects>}, {"buses", &buses}, {"busStats", &lua::Binding::native<&busStats>}, {"setListener", &lua::Binding::native<&setListener>}, {"listener", &listener}, {"followCamera", &lua::Binding::native<&followCamera>}, {"setSpatialization", &lua::Binding::native<&setSpatialization>}, {"spatialization", &spatialization}, {"sampleRate", &sampleRate}, {"channels", &channels}, {"hasDevice", &hasDevice}, {"outputAvailable", &outputAvailable}, {"outputOpening", &outputOpening}, {"outputState", &outputState}, {"outputLatency", &outputLatency}, {"outputMuted", &outputMuted}, {"restartOutput", &lua::Binding::native<&restartOutput>}, {"tapMaster", &lua::Binding::native<&TapBinding::tapMaster>}, {"setBackground", &lua::Binding::native<&setBackground>}, {"background", &background}, {nullptr, nullptr},
    };
    lua::Binding::newModule(L, functions);
    return 1;
}

void AudioBinding::install(lua_State* L) {
    lua::ClassBuilder<haylen::audio::Sound>(L).property("duration", &soundDuration).property("channels", &soundChannels).property("sampleRate", &soundSampleRate).property("frameCount", &soundFrameCount).property("streamed", &soundStreamed).meta("__eq", &lua::Userdata::equal<haylen::audio::Sound>).install();
    EffectBinding::install(L);
    TapBinding::install(L);
    lua::Binding::preload(L, "haylen.audio", &open);
}

} // namespace haylen::lua::audio
