#include "ui/ScriptedScreen.hpp"

#include <lua.hpp>

#include <exception>
#include <new>
#include <stdexcept>

#include "haylen/lua/Runtime.hpp"
#include "haylen/lua/Userdata.hpp"
#include "haylen/ui/Gui.hpp"
#include "haylen/ui/NavigationStack.hpp"
#include "haylen/ui/Presentation.hpp"
#include "runtime/Owners.hpp"
#include "ui/NavigationBinding.hpp"
#include "ui/UiBinding.hpp"

namespace haylen::lua::ui {

ScriptedScreen::ScriptedScreen(lua_State* L, int index) : table(L, index) {}

int ScriptedScreen::collectHandle(lua_State* L) {
    static_cast<std::weak_ptr<ScriptedScreen>*>(lua_touserdata(L, 1))->~weak_ptr();
    lua_pushnil(L);
    lua_setmetatable(L, 1);
    return 0;
}

// The map keeps a weak handle to the screen, so the screen lives only while the navigation or the app holds it, and its table only while the screen or the app does.
std::shared_ptr<ScriptedScreen> ScriptedScreen::get(lua_State* L, int index) {
    const int key = lua_absindex(L, index);
    luaL_checktype(L, key, LUA_TTABLE);
    lua::Owners::pushWeakTable(L, kScreens, "k");
    const int screens = lua_gettop(L);
    lua_pushvalue(L, key);
    if (lua_rawget(L, screens) == LUA_TUSERDATA) {
        if (std::shared_ptr<ScriptedScreen> known = static_cast<std::weak_ptr<ScriptedScreen>*>(lua_touserdata(L, -1))->lock()) {
            lua_pop(L, 2);
            return known;
        }
    }

    lua_pop(L, 1);
    auto created = std::make_shared<ScriptedScreen>(L, key);
    lua_pushvalue(L, key);
    if (lua::Userdata::newMetatable(L, kHandleType)) {
        lua_pushcfunction(L, &collectHandle);
        lua_setfield(L, -2, "__gc");
    }

    lua_pop(L, 1);
    new (lua_newuserdatauv(L, sizeof(std::weak_ptr<ScriptedScreen>), 0)) std::weak_ptr<ScriptedScreen>(created);
    luaL_setmetatable(L, kHandleType);
    lua_rawset(L, screens);
    lua_pop(L, 1);
    return created;
}

haylen::ui::Screen::State ScriptedScreen::findState(lua_State* L, int index) {
    const int key = lua_absindex(L, index);
    lua::Owners::pushWeakTable(L, kScreens, "k");
    lua_pushvalue(L, key);
    State found = State::Created;
    if (lua_rawget(L, -2) == LUA_TUSERDATA) {
        const std::shared_ptr<ScriptedScreen> screen = static_cast<std::weak_ptr<ScriptedScreen>*>(lua_touserdata(L, -1))->lock();
        found = screen ? screen->getState() : State::Removed;
    }

    lua_pop(L, 2);
    return found;
}

void ScriptedScreen::push(lua_State* L, const std::shared_ptr<haylen::ui::Screen>& screen) {
    const auto* scripted = dynamic_cast<const ScriptedScreen*>(screen.get());
    if (scripted == nullptr) {
        lua_pushnil(L);
        return;
    }

    scripted->table.push(L);
}

bool ScriptedScreen::call(const char* name, const float* argument) const {
    bool answer = false;
    // clang-format off
    lua::Runtime::protectedRun(table.getState(), [&](lua_State* L) {
        table.push(L);
        lua_getfield(L, -1, name);
        if (!lua_isfunction(L, -1)) {
            lua_pop(L, 2);
            return;
        }

        lua_insert(L, -2);
        if (argument != nullptr) {
            lua_pushnumber(L, *argument);
        }

        lua_call(L, argument != nullptr ? 2 : 1, 1);
        answer = lua_toboolean(L, -1) != 0;
        lua_pop(L, 1);
    });
    // clang-format on
    return answer;
}

// The tree that `build` returns becomes the GUI of the screen, which shares the focus with the GUIs around it, such as the bars of the app.
std::shared_ptr<haylen::ui::Gui> ScriptedScreen::build(haylen::core::Engine&, haylen::ui::Placement placement) {
    std::shared_ptr<haylen::ui::Gui> built;
    // clang-format off
    lua::Runtime::protectedRun(table.getState(), [&](lua_State* L) {
        table.push(L);
        const int self = lua_gettop(L);
        if (lua_getfield(L, self, "build") != LUA_TFUNCTION) {
            luaL_error(L, "A screen needs a \"build\" method that returns the node table of its GUI.");
        }

        lua_pushvalue(L, self);
        lua_call(L, 1, 1);
        if (!lua_istable(L, -1)) {
            luaL_error(L, "The \"build\" method of a screen must return a node table, not %s.", luaL_typename(L, -1));
        }

        built = UiBinding::buildGui(L, -1, placement, true);
        lua_setfield(L, self, "gui");
        lua_settop(L, self - 1);
    });
    // clang-format on
    return built;
}

void ScriptedScreen::appear(haylen::core::Engine&) {
    static_cast<void>(call("appear"));
}

void ScriptedScreen::appeared(haylen::core::Engine&) {
    static_cast<void>(call("appeared"));
}

void ScriptedScreen::disappear(haylen::core::Engine&) {
    static_cast<void>(call("disappear"));
}

void ScriptedScreen::disappeared(haylen::core::Engine&) {
    static_cast<void>(call("disappeared"));
}

void ScriptedScreen::update(haylen::core::Engine&, float deltaSeconds) {
    static_cast<void>(call("update", &deltaSeconds));
}

void ScriptedScreen::render(haylen::core::Engine&) {
    static_cast<void>(call("render"));
}

bool ScriptedScreen::back(haylen::core::Engine&) {
    return call("back");
}

// Everything the table owns ends after its remove hook, even when the hook fails, and the table no longer names a stack or a presentation.
void ScriptedScreen::remove(haylen::core::Engine& engine) {
    std::exception_ptr failure;
    try {
        static_cast<void>(call("remove"));
    } catch (...) {
        failure = std::current_exception();
    }

    // clang-format off
    lua::Runtime::protectedRun(table.getState(), [this](lua_State* L) {
        table.push(L);
        lua::Owners::release(L, -1);
        lua_pop(L, 1);
    });
    // clang-format on
    placed(engine);

    if (failure) {
        std::rethrow_exception(failure);
    }
}

void ScriptedScreen::placed(haylen::core::Engine&) {
    // clang-format off
    lua::Runtime::protectedRun(table.getState(), [this](lua_State* L) {
        table.push(L);
        NavigationBinding::pushStack(L, getStack());
        lua_setfield(L, -2, "stack");
        NavigationBinding::pushPresentation(L, getPresentation());
        lua_setfield(L, -2, "presentation");
        lua_pop(L, 1);
    });
    // clang-format on
}

} // namespace haylen::lua::ui
