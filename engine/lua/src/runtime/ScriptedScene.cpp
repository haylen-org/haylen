#include "runtime/ScriptedScene.hpp"

#include <exception>
#include <new>
#include <optional>
#include <stdexcept>

#include "core/SceneBinding.hpp"
#include "haylen/core/Engine.hpp"
#include "haylen/core/FrameClock.hpp"
#include "haylen/core/SceneManager.hpp"
#include "haylen/lua/Runtime.hpp"
#include "haylen/lua/TypeConverter.hpp"
#include "haylen/lua/Userdata.hpp"
#include "input/InputBinding.hpp"
#include "runtime/Owners.hpp"
#include "runtime/Task.hpp"
#include "runtime/WeakReference.hpp"

namespace haylen::lua {

ScriptedScene::ScriptedScene(lua_State* L, int index) : table(L, index) {}

int ScriptedScene::collectHandle(lua_State* L) {
    static_cast<std::weak_ptr<ScriptedScene>*>(lua_touserdata(L, 1))->~weak_ptr();
    lua_pushnil(L);
    lua_setmetatable(L, 1);
    return 0;
}

std::shared_ptr<ScriptedScene> ScriptedScene::find(lua_State* L, int index) {
    const int key = lua_absindex(L, index);
    Owners::pushWeakTable(L, kScenes, "k");
    lua_pushvalue(L, key);
    std::shared_ptr<ScriptedScene> scene;
    if (lua_rawget(L, -2) == LUA_TUSERDATA) {
        scene = static_cast<std::weak_ptr<ScriptedScene>*>(lua_touserdata(L, -1))->lock();
    }

    lua_pop(L, 2);
    return scene;
}

bool ScriptedScene::wasScene(lua_State* L, int index) {
    const int key = lua_absindex(L, index);
    Owners::pushWeakTable(L, kScenes, "k");
    lua_pushvalue(L, key);
    const bool known = lua_rawget(L, -2) == LUA_TUSERDATA;
    lua_pop(L, 2);
    return known;
}

// The map keeps a weak handle to the scene, so the scene lives only while the engine holds it and its table only while the scene or the app does.
std::shared_ptr<ScriptedScene> ScriptedScene::get(lua_State* L, int index) {
    const int key = lua_absindex(L, index);
    if (std::shared_ptr<ScriptedScene> known = find(L, key)) {
        return known;
    }

    auto created = std::make_shared<ScriptedScene>(L, key);
    Owners::pushWeakTable(L, kScenes, "k");
    lua_pushvalue(L, key);
    if (Userdata::newMetatable(L, kHandleType)) {
        lua_pushcfunction(L, &collectHandle);
        lua_setfield(L, -2, "__gc");
    }

    lua_pop(L, 1);
    new (lua_newuserdatauv(L, sizeof(std::weak_ptr<ScriptedScene>), 0)) std::weak_ptr<ScriptedScene>(created);
    luaL_setmetatable(L, kHandleType);
    lua_rawset(L, -3);
    lua_pop(L, 1);
    return created;
}

void ScriptedScene::pushParams(lua_State* L, const std::any& params) {
    if (!params.has_value()) {
        lua_pushnil(L);
        return;
    }

    const auto* value = std::any_cast<std::shared_ptr<Reference>>(&params);
    if (value == nullptr) {
        throw std::invalid_argument("A Lua scene takes the \"params\" of a change from Lua.");
    }

    (*value)->push(L);
}

void ScriptedScene::pushTable(lua_State* L) const {
    table.push(L);
}

void ScriptedScene::call(const char* name, int arguments, const std::function<void(lua_State*)>& pushArguments) const {
    // clang-format off
    Runtime::protectedRun(table.getState(), [&](lua_State* L) {
        table.push(L);
        lua_getfield(L, -1, name);
        if (!lua_isfunction(L, -1)) {
            lua_pop(L, 2);
            return;
        }

        lua_insert(L, -2);

        if (pushArguments) {
            pushArguments(L);
        }

        lua_call(L, 1 + arguments, 0);
    });
    // clang-format on
}

// The load hook runs as a task that the table owns, so the load holds a deferral until the task finishes and an unload cancels the task.
void ScriptedScene::load(haylen::core::Engine&, haylen::core::SceneLoad& context) {
    // clang-format off
    Runtime::protectedRun(table.getState(), [this, &context](lua_State* L) {
        table.push(L);
        const int self = lua_gettop(L);
        if (lua_getfield(L, self, "load") != LUA_TFUNCTION) {
            lua_settop(L, self - 1);
            return;
        }

        lua_pushvalue(L, self);
        lua::core::SceneBinding::pushLoad(L, context);
        auto deferral = std::make_shared<haylen::core::SceneLoad::Deferral>(context.defer());
        Task::start(L, self + 1, 2, self, [deferral](const std::optional<haylen::core::AppError>& error) {
            if (error) {
                deferral->fail(*error);
                return;
            }

            deferral->complete();
        });
        lua_settop(L, self - 1);
    });
    // clang-format on
}

void ScriptedScene::enter(haylen::core::Engine&, const std::any& params) {
    call("enter", 1, [&params](lua_State* L) { pushParams(L, params); });
}

void ScriptedScene::enterTransitionFinished(haylen::core::Engine&) {
    call("enterTransitionFinished");
}

void ScriptedScene::exitTransitionStarted(haylen::core::Engine&) {
    call("exitTransitionStarted");
}

void ScriptedScene::exit(haylen::core::Engine&) {
    call("exit");
}

// Everything the table owns ends after its unload hook, even when the hook fails.
void ScriptedScene::unload(haylen::core::Engine&) {
    std::exception_ptr failure;
    try {
        call("unload");
    } catch (...) {
        failure = std::current_exception();
    }

    // clang-format off
    Runtime::protectedRun(table.getState(), [this](lua_State* L) {
        table.push(L);
        Owners::release(L, -1);
        lua_pop(L, 1);
    });
    // clang-format on

    if (failure) {
        std::rethrow_exception(failure);
    }
}

void ScriptedScene::pause(haylen::core::Engine&) {
    call("pause");
}

void ScriptedScene::resume(haylen::core::Engine&) {
    call("resume");
}

void ScriptedScene::paused(haylen::core::Engine&) {
    call("paused");
}

void ScriptedScene::unpaused(haylen::core::Engine&) {
    call("unpaused");
}

void ScriptedScene::event(haylen::core::Engine&, const haylen::platform::Event& event) {
    call("event", 1, [&event](lua_State* L) { lua::input::InputBinding::pushEvent(L, event); });
}

void ScriptedScene::fixedUpdate(haylen::core::Engine&, float stepSeconds) {
    call("fixedUpdate", 1, [stepSeconds](lua_State* L) { lua_pushnumber(L, stepSeconds); });
}

void ScriptedScene::update(haylen::core::Engine&, float deltaSeconds) {
    call("update", 1, [deltaSeconds](lua_State* L) { lua_pushnumber(L, deltaSeconds); });
}

void ScriptedScene::render(haylen::core::Engine&) {
    call("render");
}

void ScriptedScene::renderUi(haylen::core::Engine&) {
    call("renderUi");
}

haylen::core::ProcessMode ScriptedScene::readProcessMode(lua_State* L, int index) {
    if (lua_getfield(L, index, "processMode") == LUA_TNIL) {
        lua_pop(L, 1);
        return haylen::core::ProcessMode::Inherit;
    }

    const std::optional<haylen::core::ProcessMode> mode = lua_type(L, -1) == LUA_TSTRING ? haylen::core::EnumNames<haylen::core::ProcessMode>::fromName(lua_tostring(L, -1)) : std::nullopt;
    if (!mode) {
        luaL_error(L, "The \"processMode\" must be \"inherit\", \"pausable\", \"whenPaused\", \"always\" or \"disabled\".");
    }

    lua_pop(L, 1);
    return *mode;
}

haylen::core::ProcessMode ScriptedScene::getProcessMode() const {
    haylen::core::ProcessMode mode = haylen::core::ProcessMode::Inherit;
    // clang-format off
    Runtime::protectedRun(table.getState(), [&](lua_State* L) {
        table.push(L);
        mode = readProcessMode(L, -1);
        lua_pop(L, 1);
    });
    // clang-format on
    return mode;
}

haylen::core::ProcessMode ScriptedScene::resolveOwnerMode(lua_State* L, int owner) {
    const int index = lua_absindex(L, owner);
    if (!lua_istable(L, index)) {
        return haylen::core::ProcessMode::Inherit;
    }

    const haylen::core::SceneManager& scenes = Runtime::getEngine(L).getScenes();
    if (const std::shared_ptr<ScriptedScene> scene = find(L, index)) {
        if (const std::optional<std::size_t> level = scenes.find(*scene)) {
            return scenes.getProcessMode(*level);
        }
    }

    return readProcessMode(L, index);
}

// Every timer and tween of an owner shares the mode of the owner, which the owner gives once per frame however much it owns, and which each holder keeps alive while the owner is weakly kept.
std::shared_ptr<ScriptedScene::OwnerMode> ScriptedScene::findOwnerMode(lua_State* L, int owner) {
    const int key = lua_absindex(L, owner);
    Owners::pushWeakTable(L, kOwnerModes, "k");
    lua_pushvalue(L, key);
    if (lua_rawget(L, -2) == LUA_TUSERDATA) {
        std::shared_ptr<OwnerMode> found = *static_cast<std::shared_ptr<OwnerMode>*>(lua_touserdata(L, -1));
        lua_pop(L, 2);
        return found;
    }

    lua_pop(L, 1);

    auto created = std::make_shared<OwnerMode>(L, key);

    if (Userdata::newMetatable(L, kOwnerModeType)) {
        lua_pushcfunction(L, &collectOwnerMode);
        lua_setfield(L, -2, "__gc");
    }

    lua_pop(L, 1);
    new (lua_newuserdatauv(L, sizeof(std::shared_ptr<OwnerMode>), 0)) std::shared_ptr<OwnerMode>(created);
    luaL_setmetatable(L, kOwnerModeType);
    lua_pushvalue(L, key);
    lua_insert(L, -2);
    lua_rawset(L, -3);
    lua_pop(L, 1);
    return created;
}

int ScriptedScene::collectOwnerMode(lua_State* L) {
    static_cast<std::shared_ptr<OwnerMode>*>(lua_touserdata(L, 1))->~shared_ptr();
    lua_pushnil(L);
    lua_setmetatable(L, 1);
    return 0;
}

std::function<haylen::core::ProcessMode()> ScriptedScene::followOwnerMode(lua_State* L, int owner) {
    std::shared_ptr<OwnerMode> shared = findOwnerMode(L, owner);
    lua_State* main = Runtime::getMainThread(L);
    const haylen::core::FrameClock& clock = Runtime::getEngine(L).getClock();
    // clang-format off
    return [shared = std::move(shared), main, &clock] {
        OwnerMode& cached = *shared;
        if (cached.frame == clock.getFrameIndex()) {
            return cached.mode;
        }

        haylen::core::ProcessMode mode = haylen::core::ProcessMode::Inherit;
        Runtime::protectedRun(main, [&](lua_State* thread) {
            if (cached.owner.push(thread)) {
                mode = resolveOwnerMode(thread, -1);
                lua_pop(thread, 1);
            }
        });
        cached.frame = clock.getFrameIndex();
        cached.mode = mode;
        return mode;
    };
    // clang-format on
}

// A table made by a class of `haylen.class` reads the name of its class through its metatable.
std::string ScriptedScene::getName() const {
    std::string name = haylen::core::Scene::getName();
    // clang-format off
    Runtime::protectedRun(table.getState(), [&](lua_State* L) {
        table.push(L);
        if (lua_getfield(L, -1, "name") == LUA_TSTRING) {
            name = lua_tostring(L, -1);
        }

        lua_pop(L, 2);
    });
    // clang-format on
    return name;
}

bool ScriptedScene::isTransparent() const {
    bool result = false;
    // clang-format off
    Runtime::protectedRun(table.getState(), [&](lua_State* L) {
        table.push(L);
        lua_getfield(L, -1, "transparent");
        result = lua_toboolean(L, -1) != 0;
        lua_pop(L, 2);
    });
    // clang-format on
    return result;
}

} // namespace haylen::lua
