#include "platform/DialogsLua.hpp"

#include <lua.hpp>

#include <cmath>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>

#include "haylen/core/Engine.hpp"
#include "haylen/lua/Binding.hpp"
#include "haylen/lua/EnumNames.hpp"
#include "haylen/lua/Runtime.hpp"
#include "haylen/lua/Stack.hpp"
#include "haylen/lua/Table.hpp"
#include "haylen/lua/Userdata.hpp"
#include "haylen/platform/Dialogs.hpp"
#include "platform/PlatformLua.hpp"
#include "varn/async/Promise.h"

namespace haylen::lua {

template <> struct EnumNames<platform::DialogRequest::MessageKind> {
    static std::optional<platform::DialogRequest::MessageKind> fromName(std::string_view name) {
        return platform::DialogRequest::messageKindFromName(name);
    }
    static std::string_view name(platform::DialogRequest::MessageKind value) {
        return platform::DialogRequest::messageKindName(value);
    }
};

} // namespace haylen::lua

namespace haylen::platform {

void DialogsLua::pushCall(lua_State* L, const DialogRequest& request, int options) {
    core::Engine& owner = lua::Runtime::getEngine(L);
    const std::optional<std::chrono::steady_clock::duration> timeout = readTimeout(L, options);
    auto pending = std::make_shared<PlatformLua::Call>();
    pending->promise = std::make_shared<varn::async::Promise>(owner.getScriptRuntime());
    pending->cancel = &cancelDialog;

    // clang-format off
    pending->id = owner.getDialogs().show(request, [pending](DialogResult result) {
        if (result.failure) {
            pending->error = Bridge::Error{.message = result.failure->message, .code = std::string(DialogResult::codeName(result.failure->code))};
            pending->promise->reject(pending->error->message);
            return;
        }
        pending->promise->resolveCustom([choice = std::move(result)](lua_State* state) { pushChoice(state, choice); });
    }, timeout);
    // clang-format on

    lua::Userdata::emplace<PlatformLua::Call>(L, std::move(pending));
}

std::optional<std::chrono::steady_clock::duration> DialogsLua::readTimeout(lua_State* L, int options) {
    if (lua_isnoneornil(L, options)) {
        return std::nullopt;
    }
    std::optional<double> seconds;
    lua::Table::readField(L, options, "timeout", seconds);
    if (!seconds) {
        return std::nullopt;
    }
    if (!std::isfinite(*seconds) || *seconds <= 0.0) {
        throw std::invalid_argument("The timeout of a dialog is a positive number of seconds.");
    }
    return std::chrono::duration_cast<std::chrono::steady_clock::duration>(std::chrono::duration<double>(*seconds));
}

std::vector<DialogRequest::Filter> DialogsLua::readFilters(lua_State* L, int options) {
    std::vector<DialogRequest::Filter> filters;
    if (lua_getfield(L, options, "filters") == LUA_TNIL) {
        lua_pop(L, 1);
        return filters;
    }
    const int list = lua_gettop(L);
    if (!lua_istable(L, list)) {
        throw std::invalid_argument("The filters of a file dialog are a list of tables with a name and a list of extensions.");
    }

    const lua_Integer count = luaL_len(L, list);
    for (lua_Integer index = 1; index <= count; ++index) {
        lua_rawgeti(L, list, index);
        const int entry = lua_gettop(L);
        if (!lua_istable(L, entry)) {
            throw std::invalid_argument("The filters of a file dialog are a list of tables with a name and a list of extensions.");
        }
        lua::Table::checkFields(L, entry, {kFilterFields});
        DialogRequest::Filter filter;
        lua::Table::readField(L, entry, "name", filter.name);
        lua::Table::readField(L, entry, "extensions", filter.extensions);
        filters.push_back(std::move(filter));
        lua_pop(L, 1);
    }
    lua_pop(L, 1);
    return filters;
}

void DialogsLua::pushChoice(lua_State* L, const DialogResult& result) {
    if (result.button) {
        lua::Stack::push(L, *result.button + 1);
        return;
    }
    if (!result.files.empty()) {
        lua_createtable(L, static_cast<int>(result.files.size()), 0);
        for (std::size_t index = 0; index < result.files.size(); ++index) {
            pushFile(L, result.files[index]);
            lua_rawseti(L, -2, static_cast<lua_Integer>(index + 1));
        }
        return;
    }
    if (result.saved) {
        pushFile(L, *result.saved);
        return;
    }
    if (result.folder) {
        lua::Stack::push(L, *result.folder);
        return;
    }
    lua_pushnil(L);
}

void DialogsLua::pushFile(lua_State* L, const DialogResult::File& file) {
    lua_createtable(L, 0, 2);
    lua::Stack::push(L, file.name);
    lua_setfield(L, -2, "name");
    if (!file.path.empty()) {
        lua::Stack::push(L, file.path);
        lua_setfield(L, -2, "path");
    }
}

bool DialogsLua::cancelDialog(lua_State* L, std::uint64_t id) {
    return lua::Runtime::getEngine(L).getDialogs().cancel(id);
}

// Shows a message with `message{title, text, kind, buttons, timeout}`, whose call gives the button the user pressed, counted from one.
int DialogsLua::message(lua_State* L) {
    luaL_checktype(L, 1, LUA_TTABLE);
    lua::Table::checkFields(L, 1, {kMessageOptions});
    DialogRequest::Message dialog;
    lua::Table::readField(L, 1, "title", dialog.title);
    lua::Table::readField(L, 1, "text", dialog.text);
    lua::Table::readField(L, 1, "kind", dialog.kind);
    lua::Table::readField(L, 1, "buttons", dialog.buttons);
    pushCall(L, {.dialog = std::move(dialog)}, 1);
    return 1;
}

int DialogsLua::openFiles(lua_State* L) {
    DialogRequest::OpenFiles dialog;
    if (!lua_isnoneornil(L, 1)) {
        luaL_checktype(L, 1, LUA_TTABLE);
        lua::Table::checkFields(L, 1, {kOpenFilesOptions});
        lua::Table::readField(L, 1, "title", dialog.title);
        lua::Table::readField(L, 1, "multiple", dialog.multiple);
        dialog.filters = readFilters(L, 1);
    }
    pushCall(L, {.dialog = std::move(dialog)}, 1);
    return 1;
}

// The data to save is a string of bytes, which may be empty, and the name is the one the dialog suggests.
int DialogsLua::saveFile(lua_State* L) {
    luaL_checktype(L, 1, LUA_TTABLE);
    lua::Table::checkFields(L, 1, {kSaveFileOptions});
    DialogRequest::SaveFile dialog;
    lua::Table::readField(L, 1, "title", dialog.title);
    lua::Table::readField(L, 1, "name", dialog.name);
    dialog.filters = readFilters(L, 1);
    std::optional<std::string> data;
    lua::Table::readField(L, 1, "data", data);
    if (!data) {
        throw std::invalid_argument("A save dialog needs the data it writes, as a string.");
    }
    dialog.data.assign(data->begin(), data->end());
    pushCall(L, {.dialog = std::move(dialog)}, 1);
    return 1;
}

int DialogsLua::openFolder(lua_State* L) {
    DialogRequest::OpenFolder dialog;
    if (!lua_isnoneornil(L, 1)) {
        luaL_checktype(L, 1, LUA_TTABLE);
        lua::Table::checkFields(L, 1, {kOpenFolderOptions});
        lua::Table::readField(L, 1, "title", dialog.title);
    }
    pushCall(L, {.dialog = std::move(dialog)}, 1);
    return 1;
}

int DialogsLua::open(lua_State* L) {
    const luaL_Reg functions[] = {
        {"message", &lua::Binding::native<&message>}, {"openFiles", &lua::Binding::native<&openFiles>}, {"saveFile", &lua::Binding::native<&saveFile>}, {"openFolder", &lua::Binding::native<&openFolder>}, {nullptr, nullptr},
    };
    lua::Binding::newModule(L, functions);
    return 1;
}

void DialogsLua::install(lua_State* L) {
    lua::Binding::preload(L, "haylen.dialogs", &open);
}

} // namespace haylen::platform
