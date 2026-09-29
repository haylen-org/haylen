#include "audio/AudioLua.hpp"

#include <lua.hpp>

#include <cstdint>
#include <string>

#include "audio/EffectLua.hpp"
#include "haylen/2d/graphics/Camera.hpp"
#include "haylen/core/Engine.hpp"
#include "haylen/lua/Binding.hpp"
#include "haylen/lua/ClassBuilder.hpp"
#include "haylen/lua/Runtime.hpp"
#include "haylen/lua/Stack.hpp"
#include "haylen/lua/Table.hpp"
#include "haylen/lua/TypeConverter.hpp"
#include "haylen/lua/Userdata.hpp"

namespace haylen::audio {

Mixer& AudioLua::getMixer(lua_State* L) {
    return lua::Runtime::getEngine(L).getAudio();
}

Mixer::PlayOptions AudioLua::readPlayOptions(lua_State* L, int index) {
    Mixer::PlayOptions options;
    if (lua_isnoneornil(L, index)) {
        return options;
    }
    luaL_checktype(L, index, LUA_TTABLE);
    lua::Table::checkFields(L, index, {kPlayFields});
    lua::Table::readField(L, index, "bus", options.bus);
    lua::Table::readField(L, index, "volume", options.volume);
    lua::Table::readField(L, index, "pitch", options.pitch);
    lua::Table::readField(L, index, "pitchVariation", options.pitchVariation);
    lua::Table::readField(L, index, "pan", options.pan);
    lua::Table::readField(L, index, "loop", options.loop);
    lua::Table::readField(L, index, "fadeIn", options.fadeIn);
    lua::Table::readField(L, index, "startAt", options.startAt);
    lua::Table::readField(L, index, "processMode", options.processMode);
    options.effects = readEffects(L, index);

    // A voice becomes positional when the options give it a place in the world.
    lua_getfield(L, index, "x");
    lua_getfield(L, index, "y");
    if (!lua_isnil(L, -2) || !lua_isnil(L, -1)) {
        options.position = math::Vec2{static_cast<float>(luaL_optnumber(L, -2, 0.0)), static_cast<float>(luaL_optnumber(L, -1, 0.0))};
    }
    lua_pop(L, 2);
    return options;
}

std::vector<std::shared_ptr<Effect>> AudioLua::readEffects(lua_State* L, int index) {
    std::vector<std::shared_ptr<Effect>> list;
    lua_getfield(L, index, "effects");
    if (lua_isnil(L, -1)) {
        lua_pop(L, 1);
        return list;
    }
    if (!lua_istable(L, -1)) {
        luaL_error(L, "The option 'effects' must be a list of audio effects.");
    }
    const lua_Integer count = luaL_len(L, -1);
    for (lua_Integer position = 1; position <= count; ++position) {
        lua_geti(L, -1, position);
        std::shared_ptr<Effect> effect = EffectLua::test(L, -1);
        if (!effect) {
            luaL_error(L, "The option 'effects' must be a list of audio effects.");
        }
        list.push_back(std::move(effect));
        lua_pop(L, 1);
    }
    lua_pop(L, 1);
    return list;
}

void AudioLua::pushEffects(lua_State* L, const std::vector<std::shared_ptr<Effect>>& effects) {
    lua_createtable(L, static_cast<int>(effects.size()), 0);
    lua_Integer position = 1;
    for (const std::shared_ptr<Effect>& effect : effects) {
        EffectLua::push(L, effect);
        lua_rawseti(L, -2, position++);
    }
}

// Plays a sound with play(sound, {bus, volume, pitch, pan, loop, fadeIn, startAt, x, y, processMode, effects}) and returns the voice id.
int AudioLua::play(lua_State* L) {
    lua::Stack::push(L, getMixer(L).play(lua::Stack::read<Sound>(L, 1), readPlayOptions(L, 2)));
    return 1;
}

int AudioLua::stop(lua_State* L) {
    getMixer(L).stop(lua::Stack::read<Mixer::VoiceId>(L, 1), static_cast<float>(luaL_optnumber(L, 2, 0.0)));
    return 0;
}

int AudioLua::pause(lua_State* L) {
    getMixer(L).setPaused(lua::Stack::read<Mixer::VoiceId>(L, 1), true);
    return 0;
}

int AudioLua::resume(lua_State* L) {
    getMixer(L).setPaused(lua::Stack::read<Mixer::VoiceId>(L, 1), false);
    return 0;
}

int AudioLua::paused(lua_State* L) {
    lua::Stack::push(L, getMixer(L).isPaused(lua::Stack::read<Mixer::VoiceId>(L, 1)));
    return 1;
}

int AudioLua::setVolume(lua_State* L) {
    getMixer(L).setVolume(lua::Stack::read<Mixer::VoiceId>(L, 1), lua::Stack::read<float>(L, 2));
    return 0;
}

int AudioLua::setPitch(lua_State* L) {
    getMixer(L).setPitch(lua::Stack::read<Mixer::VoiceId>(L, 1), lua::Stack::read<float>(L, 2));
    return 0;
}

int AudioLua::pitch(lua_State* L) {
    lua::Stack::push(L, getMixer(L).getPitch(lua::Stack::read<Mixer::VoiceId>(L, 1)));
    return 1;
}

int AudioLua::seedVariation(lua_State* L) {
    getMixer(L).seedVariation(lua::Stack::read<std::uint64_t>(L, 1));
    return 0;
}

int AudioLua::setPan(lua_State* L) {
    getMixer(L).setPan(lua::Stack::read<Mixer::VoiceId>(L, 1), lua::Stack::read<float>(L, 2));
    return 0;
}

int AudioLua::setPosition(lua_State* L) {
    getMixer(L).setPosition(lua::Stack::read<Mixer::VoiceId>(L, 1), {lua::Stack::read<float>(L, 2), lua::Stack::read<float>(L, 3)});
    return 0;
}

int AudioLua::processMode(lua_State* L) {
    lua::Stack::push(L, getMixer(L).getProcessMode(lua::Stack::read<Mixer::VoiceId>(L, 1)));
    return 1;
}

int AudioLua::active(lua_State* L) {
    lua::Stack::push(L, getMixer(L).isActive(lua::Stack::read<Mixer::VoiceId>(L, 1)));
    return 1;
}

int AudioLua::cursor(lua_State* L) {
    lua::Stack::push(L, getMixer(L).getCursor(lua::Stack::read<Mixer::VoiceId>(L, 1)));
    return 1;
}

int AudioLua::stopAll(lua_State* L) {
    getMixer(L).stopAll(static_cast<float>(luaL_optnumber(L, 1, 0.0)));
    return 0;
}

int AudioLua::voiceCount(lua_State* L) {
    lua::Stack::push(L, getMixer(L).getVoiceCount());
    return 1;
}

int AudioLua::pauseAll(lua_State* L) {
    getMixer(L).pauseAll();
    return 0;
}

int AudioLua::resumeAll(lua_State* L) {
    getMixer(L).resumeAll();
    return 0;
}

int AudioLua::interrupted(lua_State* L) {
    lua::Stack::push(L, getMixer(L).isInterrupted());
    return 1;
}

int AudioLua::addEffect(lua_State* L) {
    getMixer(L).addEffect(lua::Stack::read<Mixer::VoiceId>(L, 1), EffectLua::check(L, 2));
    return 0;
}

int AudioLua::removeEffect(lua_State* L) {
    getMixer(L).removeEffect(lua::Stack::read<Mixer::VoiceId>(L, 1), *EffectLua::check(L, 2));
    return 0;
}

int AudioLua::effects(lua_State* L) {
    pushEffects(L, getMixer(L).getEffects(lua::Stack::read<Mixer::VoiceId>(L, 1)));
    return 1;
}

// Crossfades to a music track with playMusic(sound, {bus, volume, fade, loop}) and returns the voice id of the track.
int AudioLua::playMusic(lua_State* L) {
    Mixer::MusicOptions options;
    if (!lua_isnoneornil(L, 2)) {
        luaL_checktype(L, 2, LUA_TTABLE);
        lua::Table::checkFields(L, 2, {kMusicFields});
        lua::Table::readField(L, 2, "bus", options.bus);
        lua::Table::readField(L, 2, "volume", options.volume);
        lua::Table::readField(L, 2, "fade", options.fade);
        lua::Table::readField(L, 2, "loop", options.loop);
    }
    lua::Stack::push(L, getMixer(L).playMusic(lua::Stack::read<Sound>(L, 1), options));
    return 1;
}

int AudioLua::stopMusic(lua_State* L) {
    getMixer(L).stopMusic(static_cast<float>(luaL_optnumber(L, 1, 1.0)));
    return 0;
}

int AudioLua::music(lua_State* L) {
    const Sound current = getMixer(L).getMusic();
    if (!current.isValid()) {
        lua_pushnil(L);
        return 1;
    }
    lua::Stack::push(L, current);
    return 1;
}

int AudioLua::createBus(lua_State* L) {
    getMixer(L).createBus(lua::Stack::read<std::string>(L, 1), lua_isnoneornil(L, 2) ? std::string_view("master") : lua::Stack::read<std::string_view>(L, 2));
    return 0;
}

int AudioLua::setBusVolume(lua_State* L) {
    getMixer(L).setBusVolume(lua::Stack::read<std::string_view>(L, 1), lua::Stack::read<float>(L, 2), static_cast<float>(luaL_optnumber(L, 3, 0.0)));
    return 0;
}

int AudioLua::busVolume(lua_State* L) {
    lua::Stack::push(L, getMixer(L).getBusVolume(lua::Stack::read<std::string_view>(L, 1)));
    return 1;
}

int AudioLua::setBusMuted(lua_State* L) {
    getMixer(L).setBusMuted(lua::Stack::read<std::string_view>(L, 1), lua::Stack::read<bool>(L, 2));
    return 0;
}

int AudioLua::busMuted(lua_State* L) {
    lua::Stack::push(L, getMixer(L).isBusMuted(lua::Stack::read<std::string_view>(L, 1)));
    return 1;
}

int AudioLua::setBusProcessMode(lua_State* L) {
    getMixer(L).setBusProcessMode(lua::Stack::read<std::string_view>(L, 1), lua::Stack::read<core::ProcessMode>(L, 2));
    return 0;
}

int AudioLua::busProcessMode(lua_State* L) {
    lua::Stack::push(L, getMixer(L).getBusProcessMode(lua::Stack::read<std::string_view>(L, 1)));
    return 1;
}

int AudioLua::addBusEffect(lua_State* L) {
    getMixer(L).addBusEffect(lua::Stack::read<std::string_view>(L, 1), EffectLua::check(L, 2));
    return 0;
}

int AudioLua::removeBusEffect(lua_State* L) {
    getMixer(L).removeBusEffect(lua::Stack::read<std::string_view>(L, 1), *EffectLua::check(L, 2));
    return 0;
}

int AudioLua::busEffects(lua_State* L) {
    pushEffects(L, getMixer(L).getBusEffects(lua::Stack::read<std::string_view>(L, 1)));
    return 1;
}

int AudioLua::buses(lua_State* L) {
    lua::Stack::push(L, getMixer(L).getBuses());
    return 1;
}

// Lists every bus in alphabetical order as {name, voices, playing, paused, processing}.
int AudioLua::busStats(lua_State* L) {
    const std::vector<Mixer::BusStats> stats = getMixer(L).getBusStats();
    lua_createtable(L, static_cast<int>(stats.size()), 0);
    lua_Integer position = 1;
    for (const Mixer::BusStats& bus : stats) {
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

int AudioLua::setListener(lua_State* L) {
    getMixer(L).setListener({lua::Stack::read<float>(L, 1), lua::Stack::read<float>(L, 2)});
    return 0;
}

int AudioLua::listener(lua_State* L) {
    const math::Vec2 position = getMixer(L).getListener();
    lua::Stack::push(L, position.x);
    lua::Stack::push(L, position.y);
    return 2;
}

// Follows a camera with followCamera(camera), or stops following with followCamera(nil).
int AudioLua::followCamera(lua_State* L) {
    const graphics2d::Camera* camera = lua_isnoneornil(L, 1) ? nullptr : &lua::Userdata::check<graphics2d::Camera>(L, 1);
    lua_settop(L, 1);
    lua_setfield(L, LUA_REGISTRYINDEX, kCameraKey);
    getMixer(L).followCamera(camera);
    return 0;
}

// Changes the fields that setSpatialization({model, minDistance, maxDistance, rolloff, panDistance, doppler, speedOfSound}) names and keeps the others.
int AudioLua::setSpatialization(lua_State* L) {
    luaL_checktype(L, 1, LUA_TTABLE);
    lua::Table::checkFields(L, 1, {kSpatializationFields});
    Mixer& mixer = getMixer(L);
    Mixer::Spatialization settings = mixer.getSpatialization();
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

int AudioLua::spatialization(lua_State* L) {
    const Mixer::Spatialization& settings = getMixer(L).getSpatialization();
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

int AudioLua::sampleRate(lua_State* L) {
    lua::Stack::push(L, getMixer(L).getSampleRate());
    return 1;
}

int AudioLua::channels(lua_State* L) {
    lua::Stack::push(L, getMixer(L).getChannels());
    return 1;
}

int AudioLua::hasDevice(lua_State* L) {
    lua::Stack::push(L, getMixer(L).hasDevice());
    return 1;
}

int AudioLua::soundChannels(lua_State* L) {
    lua::Stack::push(L, lua::Userdata::check<Sound>(L, 1).getChannels());
    return 1;
}

int AudioLua::soundSampleRate(lua_State* L) {
    lua::Stack::push(L, lua::Userdata::check<Sound>(L, 1).getSampleRate());
    return 1;
}

int AudioLua::soundFrames(lua_State* L) {
    lua::Stack::push(L, lua::Userdata::check<Sound>(L, 1).getFrameCount());
    return 1;
}

int AudioLua::soundStreamed(lua_State* L) {
    lua::Stack::push(L, lua::Userdata::check<Sound>(L, 1).isStreamed());
    return 1;
}

int AudioLua::soundDuration(lua_State* L) {
    lua::Stack::push(L, lua::Userdata::check<Sound>(L, 1).getDuration());
    return 1;
}

int AudioLua::open(lua_State* L) {
    const luaL_Reg functions[] = {
        {"play", &lua::Binding::native<&play>}, {"stop", &lua::Binding::native<&stop>}, {"pause", &lua::Binding::native<&pause>}, {"resume", &lua::Binding::native<&resume>}, {"paused", &lua::Binding::native<&paused>}, {"setVolume", &lua::Binding::native<&setVolume>}, {"setPitch", &lua::Binding::native<&setPitch>}, {"pitch", &lua::Binding::native<&pitch>}, {"seedVariation", &lua::Binding::native<&seedVariation>}, {"setPan", &lua::Binding::native<&setPan>}, {"setPosition", &lua::Binding::native<&setPosition>}, {"processMode", &lua::Binding::native<&processMode>}, {"active", &lua::Binding::native<&active>}, {"cursor", &lua::Binding::native<&cursor>}, {"stopAll", &lua::Binding::native<&stopAll>}, {"voiceCount", &voiceCount}, {"pauseAll", &pauseAll}, {"resumeAll", &resumeAll}, {"interrupted", &interrupted}, {"newEffect", &lua::Binding::native<&EffectLua::newEffect>}, {"addEffect", &lua::Binding::native<&addEffect>}, {"removeEffect", &lua::Binding::native<&removeEffect>}, {"effects", &lua::Binding::native<&effects>}, {"playMusic", &lua::Binding::native<&playMusic>}, {"stopMusic", &lua::Binding::native<&stopMusic>}, {"music", &music}, {"createBus", &lua::Binding::native<&createBus>}, {"setBusVolume", &lua::Binding::native<&setBusVolume>}, {"busVolume", &lua::Binding::native<&busVolume>}, {"setBusMuted", &lua::Binding::native<&setBusMuted>}, {"busMuted", &lua::Binding::native<&busMuted>}, {"setBusProcessMode", &lua::Binding::native<&setBusProcessMode>}, {"busProcessMode", &lua::Binding::native<&busProcessMode>}, {"addBusEffect", &lua::Binding::native<&addBusEffect>}, {"removeBusEffect", &lua::Binding::native<&removeBusEffect>}, {"busEffects", &lua::Binding::native<&busEffects>}, {"buses", &buses}, {"busStats", &lua::Binding::native<&busStats>}, {"setListener", &lua::Binding::native<&setListener>}, {"listener", &listener}, {"followCamera", &lua::Binding::native<&followCamera>}, {"setSpatialization", &lua::Binding::native<&setSpatialization>}, {"spatialization", &spatialization}, {"sampleRate", &sampleRate}, {"channels", &channels}, {"hasDevice", &hasDevice}, {nullptr, nullptr},
    };
    lua::Binding::newModule(L, functions);
    return 1;
}

void AudioLua::install(lua_State* L) {
    lua::ClassBuilder<Sound>(L).property("duration", &soundDuration).property("channels", &soundChannels).property("sampleRate", &soundSampleRate).property("frames", &soundFrames).property("streamed", &soundStreamed).meta("__eq", &lua::Userdata::equal<Sound>).install();
    EffectLua::install(L);
    lua::Binding::preload(L, "haylen.audio", &open);
}

} // namespace haylen::audio
