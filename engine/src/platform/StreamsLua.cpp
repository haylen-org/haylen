#include "platform/StreamsLua.hpp"

#include <lua.hpp>

#include <cstdint>
#include <utility>

#include "audio/AudioLua.hpp"
#include "core/FloatBufferLua.hpp"
#include "haylen/core/Engine.hpp"
#include "haylen/lua/Binding.hpp"
#include "haylen/lua/ClassBuilder.hpp"
#include "haylen/lua/Runtime.hpp"
#include "haylen/lua/Stack.hpp"
#include "haylen/lua/Table.hpp"
#include "haylen/lua/TypeConverter.hpp"
#include "haylen/lua/Userdata.hpp"
#include "haylen/platform/PluginStreams.hpp"
#include "lua/Owners.hpp"
#include "plugins/PlatformPlugin.hpp"

namespace haylen::platform {

void StreamsLua::pushVideoStream(lua_State* L, const std::string& plugin, const std::string& name) {
    std::shared_ptr<VideoStream> stream = PluginStreams::findVideo(plugin, name);
    if (!stream) {
        lua_pushnil(L);
        return;
    }
    lua::Runtime::getEngine(L).getPlugin<plugins::PlatformPlugin>().watch(stream);
    lua::Userdata::emplace<VideoStream>(L, std::move(stream));
}

void StreamsLua::pushAudioStream(lua_State* L, const std::string& plugin, const std::string& name) {
    std::shared_ptr<AudioStream> stream = PluginStreams::findAudio(plugin, name);
    if (!stream) {
        lua_pushnil(L);
        return;
    }
    lua::Userdata::emplace<AudioStream>(L, std::move(stream));
}

int StreamsLua::getTexture(lua_State* L) {
    lua::Stack::push(L, lua::Userdata::check<VideoStream>(L, 1).getTexture(lua::Runtime::getEngine(L).getGraphics()));
    return 1;
}

int StreamsLua::getWidth(lua_State* L) {
    lua::Stack::push(L, lua::Userdata::check<VideoStream>(L, 1).getWidth());
    return 1;
}

int StreamsLua::getHeight(lua_State* L) {
    lua::Stack::push(L, lua::Userdata::check<VideoStream>(L, 1).getHeight());
    return 1;
}

int StreamsLua::getFrameCount(lua_State* L) {
    lua::Stack::push(L, lua::Userdata::check<VideoStream>(L, 1).getFrameCount());
    return 1;
}

int StreamsLua::getTimestamp(lua_State* L) {
    lua::Stack::push(L, lua::Userdata::check<VideoStream>(L, 1).getTimestamp());
    return 1;
}

// Listens with on('frame', function(timestamp), {owner}) to every frame the texture receives, and returns a connection. A listener with an owner ends with it, like the listeners of events and signals.
int StreamsLua::on(lua_State* L) {
    VideoStream& target = lua::Userdata::check<VideoStream>(L, 1);
    const std::string_view event = lua::Stack::read<std::string_view>(L, 2);
    if (event != "frame") {
        return luaL_error(L, "Unknown video stream event '%s'. Video streams report frame.", std::string(event).c_str());
    }
    luaL_checktype(L, 3, LUA_TFUNCTION);
    int owner = 0;
    if (!lua_isnoneornil(L, 4)) {
        luaL_checktype(L, 4, LUA_TTABLE);
        lua::Table::checkFields(L, 4, {kListenerOptions});
        if (lua_getfield(L, 4, "owner") != LUA_TNIL) {
            owner = lua_gettop(L);
        }
    }
    const std::weak_ptr<const void> lifetime = owner != 0 ? lua::Owners::getLifetime(L, owner) : std::weak_ptr<const void>();
    auto function = std::make_shared<lua::Owners::Function>(L, 3, owner);
    lua_State* main = lua::Runtime::getMainThread(L);

    // clang-format off
    core::Connection connection = target.frameReceived.connect([function, main](double timestamp) {
        lua::Runtime::runReporting(main, [&] {
            if (function->push(main)) {
                lua_pushnumber(main, timestamp);
                lua::Runtime::protectedCall(main, 1, 0);
            }
        });
    }, {.owner = lifetime});
    // clang-format on
    if (owner != 0) {
        lua::Owners::add(L, owner, connection);
    }
    lua::Userdata::emplace<core::Connection>(L, std::move(connection));
    return 1;
}

// Plays the stream as a voice with play({bus, volume, pan, fadeIn, x, y, processMode, effects}) and returns the voice id. A new voice of the stream takes it over from the voice that played it before.
int StreamsLua::play(lua_State* L) {
    const std::shared_ptr<AudioStream>& stream = lua::Userdata::checkShared<AudioStream>(L, 1);
    lua::Stack::push(L, lua::Runtime::getEngine(L).getAudio().play(stream, audio::AudioLua::readPlayOptions(L, 2, kPlayFields)));
    return 1;
}

// Copies the newest samples into a float buffer of haylen.collections with read(buffer), interleaved and oldest first from the first value, and returns how many values it wrote.
int StreamsLua::read(lua_State* L) {
    const AudioStream& stream = lua::Userdata::check<AudioStream>(L, 1);
    lua::Stack::push(L, stream.copyLatest(lua::Userdata::check<core::FloatBuffer>(L, 2).getValues()));
    return 1;
}

int StreamsLua::getSampleRate(lua_State* L) {
    lua::Stack::push(L, lua::Userdata::check<AudioStream>(L, 1).getSampleRate());
    return 1;
}

int StreamsLua::getChannels(lua_State* L) {
    lua::Stack::push(L, lua::Userdata::check<AudioStream>(L, 1).getChannels());
    return 1;
}

int StreamsLua::getUnderruns(lua_State* L) {
    lua::Stack::push(L, lua::Userdata::check<AudioStream>(L, 1).getUnderrunCount());
    return 1;
}

void StreamsLua::install(lua_State* L) {
    lua::ClassBuilder<VideoStream>(L).property("texture", &lua::Binding::native<&getTexture>).property("width", &getWidth).property("height", &getHeight).property("frameCount", &getFrameCount).property("timestamp", &getTimestamp).function("on", &lua::Binding::native<&on>).meta("__eq", &lua::Userdata::equal<VideoStream>).install();
    lua::ClassBuilder<AudioStream>(L).function("play", &lua::Binding::native<&play>).function("read", &lua::Binding::native<&read>).property("sampleRate", &getSampleRate).property("channels", &getChannels).property("underruns", &getUnderruns).meta("__eq", &lua::Userdata::equal<AudioStream>).install();
}

} // namespace haylen::platform
