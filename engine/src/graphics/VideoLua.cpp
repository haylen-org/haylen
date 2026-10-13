#include "graphics/VideoLua.hpp"

#include <lua.hpp>

#include <string>
#include <utility>

#include "haylen/core/Engine.hpp"
#include "haylen/lua/Binding.hpp"
#include "haylen/lua/ClassBuilder.hpp"
#include "haylen/lua/Promise.hpp"
#include "haylen/lua/Runtime.hpp"
#include "haylen/lua/Stack.hpp"
#include "haylen/lua/Table.hpp"
#include "haylen/lua/TypeConverter.hpp"
#include "haylen/lua/Userdata.hpp"
#include "haylen/plugins/VideoPlugin.hpp"
#include "lua/Owners.hpp"

namespace haylen::graphics {

void VideoLua::Release::disconnect() {
    if (const std::shared_ptr<Video> alive = video.lock()) {
        lua::Runtime::getEngine(main).getPlugin<plugins::VideoPlugin>().release(*alive);
    }
    video.reset();
}

// Reads `path` and the options at index 2, and ties the video to its owner, which releases it when it ends.
std::shared_ptr<Video> VideoLua::create(lua_State* L) {
    const auto path = lua::Stack::read<std::string>(L, 1);
    Video::Options options;
    int owner = 0;
    if (!lua_isnoneornil(L, 2)) {
        luaL_checktype(L, 2, LUA_TTABLE);
        lua::Table::checkFields(L, 2, {kLoadOptions});
        lua::Table::readField(L, 2, "bus", options.bus);
        lua::Table::readField(L, 2, "volume", options.volume);
        lua::Table::readField(L, 2, "muted", options.muted);
        lua::Table::readField(L, 2, "loop", options.loop);
        lua::Table::readField(L, 2, "rate", options.rate);
        lua::Table::readField(L, 2, "autoplay", options.autoplay);
        lua::Table::readField(L, 2, "processMode", options.processMode);
        if (lua_getfield(L, 2, "owner") != LUA_TNIL) {
            owner = lua_gettop(L);
        }
    }
    if (owner == 0) {
        owner = lua::Owners::pushDefault(L);
    }
    core::Engine& engine = lua::Runtime::getEngine(L);
    std::shared_ptr<Video> video = engine.getPlugin<plugins::VideoPlugin>().create(engine, path, options);
    if (owner != 0) {
        lua::Owners::add(L, owner, std::make_shared<Release>(lua::Runtime::getMainThread(L), video));
    }
    return video;
}

// Returns a video that loads in the background, which reports `ready` or `error` once it opened.
int VideoLua::load(lua_State* L) {
    std::shared_ptr<Video> video = create(L);
    lua::Userdata::emplace<Video>(L, std::move(video));
    return 1;
}

// Returns a promise that resolves with the video once it is ready, or rejects with the reason it failed.
int VideoLua::loadAsync(lua_State* L) {
    core::Engine& engine = lua::Runtime::getEngine(L);
    std::shared_ptr<Video> video = create(L);
    const lua::Promise promise(engine);
    const std::weak_ptr<Video> weak = video;
    // clang-format off
    auto connections = std::make_shared<std::pair<core::Connection, core::Connection>>();
    connections->first = video->ready.connect([promise, weak, connections] {
        if (const std::shared_ptr<Video> alive = weak.lock()) {
            promise.resolveWith([alive](lua_State* state) { lua::Userdata::emplace<Video>(state, alive); });
        }
        connections->first.disconnect();
        connections->second.disconnect();
    });
    connections->second = video->failed.connect([promise, connections](const std::string& message) {
        promise.reject(message);
        connections->first.disconnect();
        connections->second.disconnect();
    });
    // clang-format on
    promise.push(L);
    return 1;
}

int VideoLua::play(lua_State* L) {
    lua::Userdata::check<Video>(L, 1).play();
    return 0;
}

int VideoLua::pause(lua_State* L) {
    lua::Userdata::check<Video>(L, 1).pause();
    return 0;
}

int VideoLua::seek(lua_State* L) {
    lua::Userdata::check<Video>(L, 1).seek(lua::Stack::read<double>(L, 2));
    return 0;
}

int VideoLua::release(lua_State* L) {
    lua::Runtime::getEngine(L).getPlugin<plugins::VideoPlugin>().release(lua::Userdata::check<Video>(L, 1));
    return 0;
}

// Listens with `on(event, function, {owner})` to `ready`, `ended` and `error`, whose listener receives a table with the `message` and the `code`, `unsupported` where the platform has no decoder and `failed` otherwise. A listener with an owner ends with it, like the listeners of events and signals.
int VideoLua::on(lua_State* L) {
    Video& video = lua::Userdata::check<Video>(L, 1);
    const auto event = lua::Stack::read<std::string_view>(L, 2);
    if (event != "ready" && event != "ended" && event != "error") {
        return luaL_error(L, "Unknown video event \"%s\". Videos report \"ready\", \"ended\" and \"error\".", std::string(event).c_str());
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
    if (owner == 0) {
        owner = lua::Owners::pushDefault(L);
    }
    const std::weak_ptr<const void> lifetime = owner != 0 ? lua::Owners::getLifetime(L, owner) : std::weak_ptr<const void>();
    auto function = std::make_shared<lua::Owners::Function>(L, 3, owner);
    lua_State* main = lua::Runtime::getMainThread(L);

    // clang-format off
    core::Connection connection;
    if (event == "error") {
        connection = video.failed.connect([function, main, &video](const std::string& message) {
            lua::Runtime::runReporting(main, [&] {
                if (function->push(main)) {
                    lua_createtable(main, 0, 2);
                    lua::Stack::push(main, message);
                    lua_setfield(main, -2, "message");
                    lua::Stack::push(main, std::string_view(video.isUnsupported() ? "unsupported" : "failed"));
                    lua_setfield(main, -2, "code");
                    lua::Runtime::protectedCall(main, 1, 0);
                }
            });
        }, {.owner = lifetime});
    } else {
        core::Signal<>& signal = event == "ready" ? video.ready : video.ended;
        connection = signal.connect([function, main] {
            lua::Runtime::runReporting(main, [&] {
                if (function->push(main)) {
                    lua::Runtime::protectedCall(main, 0, 0);
                }
            });
        }, {.owner = lifetime});
    }
    // clang-format on
    if (owner != 0) {
        lua::Owners::add(L, owner, connection);
    }
    lua::Userdata::emplace<core::Connection>(L, std::move(connection));
    return 1;
}

int VideoLua::getTexture(lua_State* L) {
    lua::Stack::push(L, lua::Userdata::check<Video>(L, 1).getTexture());
    return 1;
}

int VideoLua::getWidth(lua_State* L) {
    lua::Stack::push(L, lua::Userdata::check<Video>(L, 1).getWidth());
    return 1;
}

int VideoLua::getHeight(lua_State* L) {
    lua::Stack::push(L, lua::Userdata::check<Video>(L, 1).getHeight());
    return 1;
}

int VideoLua::getDuration(lua_State* L) {
    lua::Stack::push(L, lua::Userdata::check<Video>(L, 1).getDuration());
    return 1;
}

int VideoLua::getTime(lua_State* L) {
    lua::Stack::push(L, lua::Userdata::check<Video>(L, 1).getTime());
    return 1;
}

int VideoLua::setTime(lua_State* L) {
    lua::Userdata::check<Video>(L, 1).seek(lua::Stack::read<double>(L, 3));
    return 0;
}

int VideoLua::isPlaying(lua_State* L) {
    lua::Stack::push(L, lua::Userdata::check<Video>(L, 1).isPlaying());
    return 1;
}

int VideoLua::isEnded(lua_State* L) {
    lua::Stack::push(L, lua::Userdata::check<Video>(L, 1).isEnded());
    return 1;
}

int VideoLua::getState(lua_State* L) {
    lua::Stack::push(L, kStateNames[static_cast<std::size_t>(lua::Userdata::check<Video>(L, 1).getState())]);
    return 1;
}

int VideoLua::getError(lua_State* L) {
    const Video& video = lua::Userdata::check<Video>(L, 1);
    if (video.getError().empty()) {
        lua_pushnil(L);
    } else {
        lua::Stack::push(L, video.getError());
    }
    return 1;
}

int VideoLua::getFrameCount(lua_State* L) {
    lua::Stack::push(L, lua::Userdata::check<Video>(L, 1).getFrameCount());
    return 1;
}

int VideoLua::isLooping(lua_State* L) {
    lua::Stack::push(L, lua::Userdata::check<Video>(L, 1).isLooping());
    return 1;
}

int VideoLua::setLoop(lua_State* L) {
    lua::Userdata::check<Video>(L, 1).setLoop(lua::Stack::read<bool>(L, 3));
    return 0;
}

int VideoLua::getRate(lua_State* L) {
    lua::Stack::push(L, lua::Userdata::check<Video>(L, 1).getRate());
    return 1;
}

int VideoLua::setRate(lua_State* L) {
    lua::Userdata::check<Video>(L, 1).setRate(lua::Stack::read<float>(L, 3));
    return 0;
}

int VideoLua::getVolume(lua_State* L) {
    lua::Stack::push(L, lua::Userdata::check<Video>(L, 1).getVolume());
    return 1;
}

int VideoLua::setVolume(lua_State* L) {
    lua::Userdata::check<Video>(L, 1).setVolume(lua::Stack::read<float>(L, 3));
    return 0;
}

int VideoLua::isMuted(lua_State* L) {
    lua::Stack::push(L, lua::Userdata::check<Video>(L, 1).isMuted());
    return 1;
}

int VideoLua::setMuted(lua_State* L) {
    lua::Userdata::check<Video>(L, 1).setMuted(lua::Stack::read<bool>(L, 3));
    return 0;
}

int VideoLua::open(lua_State* L) {
    const luaL_Reg functions[] = {
        {"load", &lua::Binding::native<&load>},
        {"loadAsync", &lua::Binding::native<&loadAsync>},
        {nullptr, nullptr},
    };
    lua::Binding::newModule(L, functions);
    return 1;
}

void VideoLua::install(lua_State* L) {
    // clang-format off
    lua::ClassBuilder<Video>(L)
        .function("play", &lua::Binding::native<&play>)
        .function("pause", &lua::Binding::native<&pause>)
        .function("seek", &lua::Binding::native<&seek>)
        .function("release", &lua::Binding::native<&release>)
        .function("on", &lua::Binding::native<&on>)
        .property("texture", &lua::Binding::native<&getTexture>)
        .property("width", &getWidth)
        .property("height", &getHeight)
        .property("duration", &getDuration)
        .property("time", &getTime, &lua::Binding::native<&setTime>)
        .property("playing", &isPlaying)
        .property("ended", &isEnded)
        .property("state", &getState)
        .property("error", &getError)
        .property("frameCount", &getFrameCount)
        .property("loop", &isLooping, &lua::Binding::native<&setLoop>)
        .property("rate", &getRate, &lua::Binding::native<&setRate>)
        .property("volume", &getVolume, &lua::Binding::native<&setVolume>)
        .property("muted", &isMuted, &lua::Binding::native<&setMuted>)
        .meta("__eq", &lua::Userdata::equal<Video>)
        .install();
    // clang-format on
    lua::Binding::preload(L, "haylen.video", &open);
}

} // namespace haylen::graphics
