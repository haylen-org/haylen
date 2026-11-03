#include "platform/SystemLua.hpp"

#include <lua.hpp>

#include <cstring>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_set>
#include <utility>

#include "haylen/core/Engine.hpp"
#include "haylen/core/Json.hpp"
#include "haylen/core/Log.hpp"
#include "haylen/lua/Binding.hpp"
#include "haylen/lua/EnumNames.hpp"
#include "haylen/lua/JsonConverter.hpp"
#include "haylen/lua/Promise.hpp"
#include "haylen/lua/Runtime.hpp"
#include "haylen/lua/Stack.hpp"
#include "haylen/lua/Table.hpp"
#include "haylen/platform/MediaSession.hpp"
#include "haylen/platform/Permission.hpp"
#include "haylen/platform/System.hpp"

namespace haylen::lua {

template <> struct EnumNames<platform::Permission::Kind> {
    static std::optional<platform::Permission::Kind> fromName(std::string_view name) {
        return platform::Permission::kindFromName(name);
    }
    static std::string_view name(platform::Permission::Kind value) {
        return platform::Permission::kindName(value);
    }
};

} // namespace haylen::lua

namespace haylen::platform {

std::unordered_set<std::string> SystemLua::loggedRefusals;

int SystemLua::info(lua_State* L) {
    lua::JsonConverter::push(L, lua::Runtime::getEngine(L).getSystem().getInfo().toJson());
    return 1;
}

int SystemLua::theme(lua_State* L) {
    lua::Stack::push(L, System::themeName(lua::Runtime::getEngine(L).getSystem().getTheme()));
    return 1;
}

int SystemLua::battery(lua_State* L) {
    lua::JsonConverter::push(L, lua::Runtime::getEngine(L).getSystem().getBattery().toJson());
    return 1;
}

// Returns a promise that resolves on a later frame once an app took the url.
int SystemLua::openUrl(lua_State* L) {
    core::Engine& engine = lua::Runtime::getEngine(L);
    const auto url = lua::Stack::read<std::string_view>(L, 1);
    const lua::Promise promise(engine);
    engine.getSystem().openUrl(url, [&engine, promise](Permission::Answer answer) { settle(engine, promise, answer, {}); });
    promise.push(L);
    return 1;
}

// Returns a promise that resolves on a later frame with the text on the clipboard.
int SystemLua::clipboard(lua_State* L) {
    core::Engine& engine = lua::Runtime::getEngine(L);
    const lua::Promise promise(engine);
    engine.getSystem().readClipboard([&engine, promise](Permission::Answer answer, std::string text) { settle(engine, promise, answer, std::move(text)); });
    promise.push(L);
    return 1;
}

// Returns a promise that resolves on a later frame once the text reached the clipboard.
int SystemLua::setClipboard(lua_State* L) {
    core::Engine& engine = lua::Runtime::getEngine(L);
    const auto text = lua::Stack::read<std::string_view>(L, 1);
    const lua::Promise promise(engine);
    engine.getSystem().setClipboard(text, [&engine, promise](Permission::Answer answer) { settle(engine, promise, answer, {}); });
    promise.push(L);
    return 1;
}

// Returns a promise that resolves on a later frame with the name of the state, as `requestPermission` does, without asking the person.
int SystemLua::permission(lua_State* L) {
    core::Engine& engine = lua::Runtime::getEngine(L);
    const auto kind = lua::Stack::read<Permission::Kind>(L, 1);
    const lua::Promise promise(engine);
    engine.getSystem().checkPermission(kind, [&engine, promise](Permission::Answer answer) { settlePermission(engine, promise, answer); });
    promise.push(L);
    return 1;
}

// Returns a promise that resolves on a later frame with the name of the state the person left.
int SystemLua::requestPermission(lua_State* L) {
    core::Engine& engine = lua::Runtime::getEngine(L);
    const auto kind = lua::Stack::read<Permission::Kind>(L, 1);
    const lua::Promise promise(engine);
    engine.getSystem().requestPermission(kind, [&engine, promise](Permission::Answer answer) { settlePermission(engine, promise, answer); });
    promise.push(L);
    return 1;
}

// Returns a promise that resolves on a later frame once the device vibrated.
int SystemLua::vibrate(lua_State* L) {
    core::Engine& engine = lua::Runtime::getEngine(L);
    const auto seconds = lua::Stack::read<float>(L, 1);
    const lua::Promise promise(engine);
    engine.getSystem().vibrate(seconds, [&engine, promise](Permission::Answer answer) { settle(engine, promise, answer, {}); });
    promise.push(L);
    return 1;
}

// A state other than `unsupported` resolves, since the answer is the state itself. The app asked, and a missing requirement logs once on its own, so a rejection that nothing awaits never stops the app.
void SystemLua::settlePermission(core::Engine& engine, const lua::Promise& promise, const Permission::Answer& answer) {
    if (answer.state != Permission::State::Unsupported) {
        promise.resolve(std::string(Permission::stateName(answer.state)));
        return;
    }
    reject(engine, promise, answer);
}

// The call did its work on `granted`, which resolves with the value, such as the text of the clipboard, or with nothing. A refusal logs once per sentence, so a call whose promise nothing awaits, such as a vibration on a device without a motor, never stops the app and still tells the developer why.
void SystemLua::settle(core::Engine& engine, const lua::Promise& promise, const Permission::Answer& answer, std::optional<std::string> value) {
    if (answer.state == Permission::State::Granted) {
        promise.resolve(value ? core::Json(std::move(*value)) : core::Json());
        return;
    }
    if (loggedRefusals.insert(answer.message).second) {
        core::Log::warning("{}", answer.message);
    }
    reject(engine, promise, answer);
}

// Rejects with the sentence in `error`, the code `denied` for a refusal of the person or the platform and `unsupported` otherwise, and, when the project lacks requirements, those in `data.missing`.
void SystemLua::reject(core::Engine& engine, const lua::Promise& promise, const Permission::Answer& answer) {
    lua_State* main = engine.getLuaState();
    lua_createtable(main, 0, 3);
    lua::Stack::push(main, answer.message);
    lua_setfield(main, -2, "error");
    lua::Stack::push(main, answer.state == Permission::State::Denied ? "denied" : "unsupported");
    lua_setfield(main, -2, "code");
    if (!answer.missing.empty()) {
        lua_createtable(main, 0, 1);
        lua::JsonConverter::push(main, answer.missingToJson());
        lua_setfield(main, -2, "missing");
        lua_setfield(main, -2, "data");
    }
    promise.rejectHandledFailure(main);
}

// Shows what plays with `setNowPlaying({title, artist, album, duration, position, playing, artwork, commands})`, where the artwork is the bytes of a PNG or JPEG image and the commands are the names of the controls to offer.
int SystemLua::setNowPlaying(lua_State* L) {
    luaL_checktype(L, 1, LUA_TTABLE);
    lua::Table::checkFields(L, 1, {kNowPlayingFields});
    MediaSession::NowPlaying value;
    lua::Table::readField(L, 1, "title", value.title);
    lua::Table::readField(L, 1, "artist", value.artist);
    lua::Table::readField(L, 1, "album", value.album);
    lua::Table::readField(L, 1, "duration", value.duration);
    lua::Table::readField(L, 1, "position", value.position);
    lua::Table::readField(L, 1, "playing", value.playing);
    lua_getfield(L, 1, "artwork");
    if (!lua_isnil(L, -1)) {
        if (lua_type(L, -1) != LUA_TSTRING) {
            return luaL_error(L, "The option \"artwork\" must be the bytes of a PNG or JPEG image.");
        }
        std::size_t size = 0;
        const char* bytes = lua_tolstring(L, -1, &size);
        value.artwork.resize(size);
        std::memcpy(value.artwork.data(), bytes, size);
    }
    lua_pop(L, 1);
    lua_getfield(L, 1, "commands");
    if (!lua_isnil(L, -1)) {
        if (!lua_istable(L, -1)) {
            return luaL_error(L, "The option \"commands\" must be a list of command names.");
        }
        value.commands = 0;
        const auto count = static_cast<lua_Integer>(lua_rawlen(L, -1));
        for (lua_Integer index = 1; index <= count; ++index) {
            lua_rawgeti(L, -1, index);
            const std::optional<MediaSession::Command> command = lua_type(L, -1) == LUA_TSTRING ? MediaSession::commandFromName(lua_tostring(L, -1)) : std::nullopt;
            if (!command) {
                return luaL_error(L, "The option \"commands\" names \"%s\", which is not \"play\", \"pause\", \"togglePlayPause\", \"stop\", \"next\", \"previous\" or \"seek\".", luaL_tolstring(L, -1, nullptr));
            }
            value.commands |= MediaSession::bit(*command);
            lua_pop(L, 1);
        }
    }
    lua_pop(L, 1);
    lua::Runtime::getEngine(L).getMediaSession().setNowPlaying(std::move(value));
    return 0;
}

int SystemLua::clearNowPlaying(lua_State* L) {
    lua::Runtime::getEngine(L).getMediaSession().clear();
    return 0;
}

int SystemLua::open(lua_State* L) {
    const luaL_Reg functions[] = {
        {"info", &info}, {"theme", &theme}, {"battery", &battery}, {"openUrl", &lua::Binding::native<&openUrl>}, {"clipboard", &lua::Binding::native<&clipboard>}, {"setClipboard", &lua::Binding::native<&setClipboard>}, {"vibrate", &lua::Binding::native<&vibrate>}, {"permission", &lua::Binding::native<&permission>}, {"requestPermission", &lua::Binding::native<&requestPermission>}, {"setNowPlaying", &lua::Binding::native<&setNowPlaying>}, {"clearNowPlaying", &lua::Binding::native<&clearNowPlaying>}, {nullptr, nullptr},
    };
    lua::Binding::newModule(L, functions);
    return 1;
}

void SystemLua::install(lua_State* L) {
    lua::Binding::preload(L, "haylen.system", &open);
}

} // namespace haylen::platform
