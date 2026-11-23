#include "core/SceneBinding.hpp"

#include <lua.hpp>

#include <array>
#include <memory>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "core/ClassBinding.hpp"
#include "core/EventsBinding.hpp"
#include "core/SignalBinding.hpp"
#include "haylen/2d/graphics/SceneTransition.hpp"
#include "haylen/core/Engine.hpp"
#include "haylen/core/EnumNames.hpp"
#include "haylen/lua/Binding.hpp"
#include "haylen/lua/ClassBuilder.hpp"
#include "haylen/lua/Promise.hpp"
#include "haylen/lua/Reference.hpp"
#include "haylen/lua/Runtime.hpp"
#include "haylen/lua/Stack.hpp"
#include "haylen/lua/Table.hpp"
#include "haylen/lua/Type.hpp"
#include "haylen/lua/TypeConverter.hpp"
#include "haylen/lua/Userdata.hpp"
#include "runtime/Owners.hpp"
#include "runtime/ScriptedLoadingView.hpp"
#include "runtime/ScriptedScene.hpp"
#include "runtime/ScriptedTransition.hpp"
#include "runtime/Task.hpp"

namespace haylen::lua {

template <> struct Type<haylen::core::SceneLoad> {
    static constexpr const char* name = "haylen.SceneLoad";
    using Storage = std::weak_ptr<haylen::core::SceneLoad>;
};

} // namespace haylen::lua

namespace haylen::core {

template <> struct EnumNames<Scene::State> {
    static std::optional<Scene::State> fromName(std::string_view name) {
        return Scene::stateFromName(name);
    }
    static std::string_view name(Scene::State value) {
        return Scene::stateName(value);
    }
};

} // namespace haylen::core

namespace haylen::lua::core {

// A transition with a duration draws a built-in effect named by `effect`, with its direction and color, or a custom effect table, and fades through its color, black by default, without one.
haylen::core::SceneManager::Transition SceneBinding::readTransition(lua_State* L, int index) {
    haylen::core::SceneManager::Transition transition;
    if (index == 0) {
        return transition;
    }

    lua::Table::readField(L, index, "duration", transition.duration);
    lua::Table::readField(L, index, "ease", transition.ease);
    lua::Table::readField(L, index, "blockInput", transition.blockInput);
    lua::Table::readField(L, index, "reducedMotion", transition.motionReduction);

    if (lua_getfield(L, index, "effect") == LUA_TTABLE) {
        lua_getfield(L, index, "color");
        lua_getfield(L, index, "direction");
        const bool configured = !lua_isnil(L, -1) || !lua_isnil(L, -2);
        lua_pop(L, 2);

        if (configured) {
            throw std::invalid_argument("A custom transition effect takes no color or direction.");
        }

        transition.effect = std::make_shared<lua::ScriptedTransition>(L, -1);
        lua_pop(L, 1);
        return transition;
    }

    if (!lua_isnil(L, -1) && lua_type(L, -1) != LUA_TSTRING) {
        throw std::invalid_argument("The transition effect must be the name of a built-in effect or a table with a \"render\" method.");
    }

    lua_pop(L, 1);

    haylen::graphics2d::SceneTransition::Options options;
    lua::Table::readField(L, index, "effect", options.kind);
    lua::Table::readField(L, index, "direction", options.direction);
    lua::Table::readField(L, index, "color", options.color);

    if (transition.duration > 0.0F) {
        transition.effect = std::make_shared<haylen::graphics2d::SceneTransition>(options);
    }

    return transition;
}

haylen::core::SceneManager::Options SceneBinding::readOptions(lua_State* L, int index) {
    haylen::core::SceneManager::Options options{.transition = readTransition(L, index)};
    if (index == 0) {
        return options;
    }

    lua::Table::readField(L, index, "loadingDelay", options.loadingDelay);
    lua::Table::readField(L, index, "minimumLoadingTime", options.minimumLoadingTime);
    lua::Table::readField(L, index, "loadingFadeOut", options.loadingFadeOut);
    lua::Table::readField(L, index, "unloadBeforeLoad", options.unloadBeforeLoad);

    if (lua_getfield(L, index, "params") != LUA_TNIL) {
        options.params = std::make_shared<lua::Reference>(L, -1);
    }

    lua_pop(L, 1);

    if (lua_getfield(L, index, "loading") != LUA_TNIL) {
        lua::Table::requireType(L, "loading", LUA_TTABLE);
        options.loading = std::make_shared<lua::ScriptedLoadingView>(L, -1);
    }

    lua_pop(L, 1);

    // The error handler receives the message of the failure, as Lua errors carry it.
    if (lua_getfield(L, index, "onError") != LUA_TNIL) {
        lua::Table::requireType(L, "onError", LUA_TFUNCTION);
        auto handler = std::make_shared<lua::Reference>(L, -1);
        // clang-format off
        options.onError = [handler](const haylen::core::AppError& error) {
            lua_State* main = handler->getState();
            handler->push(main);
            lua_pushstring(main, error.what());
            lua::Runtime::protectedCall(main, 1, 0);
        };
        // clang-format on
    }

    lua_pop(L, 1);
    return options;
}

haylen::core::SceneManager::Completion SceneBinding::pushCompletion(lua_State* L, int options) {
    const lua::Promise promise(lua::Runtime::getEngine(L));
    std::shared_ptr<lua::Reference> callback;
    if (options != 0) {
        if (lua_getfield(L, options, "onComplete") != LUA_TNIL) {
            lua::Table::requireType(L, "onComplete", LUA_TFUNCTION);
            callback = std::make_shared<lua::Reference>(L, -1);
        }

        lua_pop(L, 1);
    }

    promise.push(L);

    // The promise resolves with `true` once the change is done and with `false` once it was dropped, and a failed load rejects it with the error, which the scenes report themselves through the event of the failed load, the error handler of a change or the log.
    // clang-format off
    return [promise, callback](const haylen::core::SceneManager::Result& result) {
        const bool completed = result.outcome == haylen::core::SceneManager::Outcome::Completed;
        if (result.error) {
            promise.rejectHandled(result.error->what());
        } else {
            promise.resolveWith([completed](lua_State* state) { lua_pushboolean(state, completed ? 1 : 0); });
        }

        if (callback) {
            lua_State* main = callback->getState();
            callback->push(main);
            lua_pushboolean(main, completed ? 1 : 0);
            lua::Runtime::protectedCall(main, 1, 0);
        }
    };
    // clang-format on
}

// Pushes the table of a Lua scene, or `false` for a scene pushed from C++, so positions in lists stay aligned.
void SceneBinding::pushScene(lua_State* L, const haylen::core::Scene& scene) {
    if (const auto* scripted = dynamic_cast<const lua::ScriptedScene*>(&scene)) {
        scripted->pushTable(L);
        return;
    }

    lua_pushboolean(L, 0);
}

void SceneBinding::pushSceneEvent(lua_State* L, const haylen::core::Scene& scene) {
    lua_createtable(L, 0, 1);
    pushScene(L, scene);
    lua_setfield(L, -2, "scene");
}

void SceneBinding::pushTransfer(lua_State* L, const haylen::core::SceneManager::Transfer& transfer) {
    lua_createtable(L, 0, 2);
    if (transfer.from != nullptr) {
        pushScene(L, *transfer.from);
        lua_setfield(L, -2, "from");
    }

    if (transfer.to != nullptr) {
        pushScene(L, *transfer.to);
        lua_setfield(L, -2, "to");
    }
}

void SceneBinding::pushLoadFailure(lua_State* L, const haylen::core::SceneManager::LoadFailure& failure) {
    lua_createtable(L, 0, 2);
    pushScene(L, *failure.scene);
    lua_setfield(L, -2, "scene");
    lua_pushstring(L, failure.error->what());
    lua_setfield(L, -2, "error");
}

void SceneBinding::pushLoad(lua_State* L, haylen::core::SceneLoad& load) {
    lua::Userdata::emplace<haylen::core::SceneLoad>(L, load.weak_from_this());
}

// Returns the index of the options table at index once its fields are checked, or 0 when the call passes no options.
int SceneBinding::checkOptions(lua_State* L, int index, std::span<const std::string_view> fields) {
    if (lua_isnoneornil(L, index)) {
        return 0;
    }

    luaL_checktype(L, index, LUA_TTABLE);
    lua::Table::checkFields(L, index, {fields});
    return index;
}

// Reads the change of a push or a replace, with the scene at index 1 and its options at index 2, and pushes the promise of the change.
haylen::core::SceneManager::Options SceneBinding::readChange(lua_State* L) {
    luaL_checktype(L, 1, LUA_TTABLE);
    (void)lua::ScriptedScene::readProcessMode(L, 1);
    const int options = checkOptions(L, 2, kChangeFields);
    haylen::core::SceneManager::Options change = readOptions(L, options);
    change.completion = pushCompletion(L, options);
    return change;
}

int SceneBinding::push(lua_State* L) {
    haylen::core::SceneManager::Options change = readChange(L);
    lua::Runtime::getEngine(L).getScenes().push(lua::ScriptedScene::get(L, 1), std::move(change));
    return 1;
}

int SceneBinding::replace(lua_State* L) {
    haylen::core::SceneManager::Options change = readChange(L);
    lua::Runtime::getEngine(L).getScenes().replace(lua::ScriptedScene::get(L, 1), std::move(change));
    return 1;
}

int SceneBinding::pop(lua_State* L) {
    const int options = checkOptions(L, 1, kTransitionFields);
    haylen::core::SceneManager::Transition transition = readTransition(L, options);
    haylen::core::SceneManager::Completion completion = pushCompletion(L, options);
    lua::Runtime::getEngine(L).getScenes().pop(std::move(transition), std::move(completion));
    return 1;
}

int SceneBinding::popTo(lua_State* L) {
    const auto level = lua::Stack::read<std::size_t>(L, 1);
    const int options = checkOptions(L, 2, kTransitionFields);
    haylen::core::SceneManager::Transition transition = readTransition(L, options);
    haylen::core::SceneManager::Completion completion = pushCompletion(L, options);
    lua::Runtime::getEngine(L).getScenes().popTo(level, std::move(transition), std::move(completion));
    return 1;
}

int SceneBinding::popToRoot(lua_State* L) {
    const int options = checkOptions(L, 1, kTransitionFields);
    haylen::core::SceneManager::Transition transition = readTransition(L, options);
    haylen::core::SceneManager::Completion completion = pushCompletion(L, options);
    lua::Runtime::getEngine(L).getScenes().popToRoot(std::move(transition), std::move(completion));
    return 1;
}

// Starts loading a scene with `preload(scene, params)` and returns a promise that resolves with `true` once it loaded, or with `false` when the preload was cancelled.
int SceneBinding::preload(lua_State* L) {
    luaL_checktype(L, 1, LUA_TTABLE);
    (void)lua::ScriptedScene::readProcessMode(L, 1);
    std::any params;
    if (!lua_isnoneornil(L, 2)) {
        params = std::make_shared<lua::Reference>(L, 2);
    }

    haylen::core::SceneManager::Completion completion = pushCompletion(L, 0);
    lua::Runtime::getEngine(L).getScenes().preload(lua::ScriptedScene::get(L, 1), std::move(params), std::move(completion));
    return 1;
}

int SceneBinding::cancelPreload(lua_State* L) {
    luaL_checktype(L, 1, LUA_TTABLE);
    const std::shared_ptr<lua::ScriptedScene> scene = lua::ScriptedScene::find(L, 1);
    if (!scene) {
        throw std::invalid_argument("The scene is not preloaded.");
    }

    lua::Runtime::getEngine(L).getScenes().cancelPreload(*scene);
    return 0;
}

int SceneBinding::clear(lua_State* L) {
    lua::Runtime::getEngine(L).getScenes().clear();
    return 0;
}

int SceneBinding::size(lua_State* L) {
    lua::Stack::push(L, lua::Runtime::getEngine(L).getScenes().size());
    return 1;
}

int SceneBinding::top(lua_State* L) {
    const haylen::core::Scene* scene = lua::Runtime::getEngine(L).getScenes().getTop();
    if (scene == nullptr || dynamic_cast<const lua::ScriptedScene*>(scene) == nullptr) {
        lua_pushnil(L);
        return 1;
    }

    pushScene(L, *scene);
    return 1;
}

// Returns the scene at a position counted from 1 at the bottom of the stack, or `nil` past its ends.
int SceneBinding::at(lua_State* L) {
    const auto index = lua::Stack::read<lua_Integer>(L, 1);
    const haylen::core::SceneManager& scenes = lua::Runtime::getEngine(L).getScenes();
    if (index < 1 || static_cast<std::size_t>(index) > scenes.size()) {
        lua_pushnil(L);
        return 1;
    }

    pushScene(L, scenes.at(static_cast<std::size_t>(index - 1)));
    return 1;
}

int SceneBinding::list(lua_State* L) {
    const haylen::core::SceneManager& scenes = lua::Runtime::getEngine(L).getScenes();
    lua_createtable(L, static_cast<int>(scenes.size()), 0);
    lua_Integer index = 0;
    for (const std::shared_ptr<haylen::core::Scene>& scene : scenes) {
        pushScene(L, *scene);
        lua_rawseti(L, -2, ++index);
    }

    return 1;
}

int SceneBinding::transitioning(lua_State* L) {
    lua::Stack::push(L, lua::Runtime::getEngine(L).getScenes().isTransitioning());
    return 1;
}

int SceneBinding::loadingViewOpacity(lua_State* L) {
    lua::Stack::push(L, lua::Runtime::getEngine(L).getScenes().getLoadingViewOpacity());
    return 1;
}

// Returns the state of the scene of a table, `unloaded` for a table whose scene the engine no longer holds, or `nil` for a table that never was a scene.
int SceneBinding::state(lua_State* L) {
    luaL_checktype(L, 1, LUA_TTABLE);

    if (const std::shared_ptr<lua::ScriptedScene> scene = lua::ScriptedScene::find(L, 1)) {
        lua::Stack::push(L, haylen::core::EnumNames<haylen::core::Scene::State>::name(scene->getState()));
        return 1;
    }

    if (lua::ScriptedScene::wasScene(L, 1)) {
        lua::Stack::push(L, haylen::core::EnumNames<haylen::core::Scene::State>::name(haylen::core::Scene::State::Unloaded));
        return 1;
    }

    lua_pushnil(L);
    return 1;
}

// Returns the progress of the load of a scene and its message.
int SceneBinding::sceneLoadProgress(lua_State* L) {
    luaL_checktype(L, 1, LUA_TTABLE);
    const std::shared_ptr<lua::ScriptedScene> scene = lua::ScriptedScene::find(L, 1);
    const haylen::core::SceneLoad::Progress current = scene ? scene->getLoadProgress() : haylen::core::SceneLoad::Progress{};
    lua_pushnumber(L, current.value);
    lua::Stack::push(L, current.message);
    return 2;
}

// Connects a function to a signal, or subscribes it to a named event, for as long as the owner lives: `scene.listen(owner, signalOrName, fn, options)`. A scene owner ends it when it unloads.
int SceneBinding::listen(lua_State* L) {
    lua::Owners::checkOwner(L, 1);
    const int options = lua_isnoneornil(L, 4) ? 0 : 4;
    if (lua_type(L, 2) == LUA_TSTRING) {
        lua::Userdata::emplace<haylen::core::Connection>(L, EventsBinding::subscribe(L, 2, 3, options, 1));
        return 1;
    }

    if (!SignalBinding::isSignal(L, 2)) {
        return luaL_typeerror(L, 2, "signal or event name");
    }

    lua::Userdata::emplace<haylen::core::Connection>(L, SignalBinding::connect(L, 2, 3, options, 1));
    return 1;
}

// Runs a function as a task that the owner holds, with `spawn(owner, fn)`: it waits on promises like a task of `async.spawn`, its errors reach the error screen with its stack, and it never resumes once the owner is released or collected.
int SceneBinding::spawn(lua_State* L) {
    lua::Owners::checkOwner(L, 1);
    luaL_checktype(L, 2, LUA_TFUNCTION);
    lua_State* main = lua::Runtime::getMainThread(L);
    // clang-format off
    lua::Task::start(L, 2, 0, 1, [main](const std::optional<haylen::core::AppError>& error) {
        if (error) {
            const haylen::core::Engine::PhaseScope scope(lua::Runtime::getEngine(main), haylen::core::Engine::Phase::Lifecycle);
            lua::Runtime::reportError(main, *error);
        }
    });
    // clang-format on

    return 0;
}

// Reports the progress of the work the scene does itself with `context:progress(value, message)`.
int SceneBinding::loadProgress(lua_State* L) {
    haylen::core::SceneLoad& load = lua::Userdata::check<haylen::core::SceneLoad>(L, 1);
    load.setProgress(lua::Stack::read<float>(L, 2), lua_isnoneornil(L, 3) ? std::string() : lua::Stack::read<std::string>(L, 3));
    return 0;
}

// Preloads one asset group or a list of them with `context:preload(groups)` and returns a promise that resolves with `true` once all of them loaded, or rejects with the first failure, which also fails the load.
int SceneBinding::loadPreload(lua_State* L) {
    haylen::core::SceneLoad& load = lua::Userdata::check<haylen::core::SceneLoad>(L, 1);
    const bool single = lua_type(L, 2) == LUA_TSTRING;
    const std::vector<std::string> groups = single ? std::vector<std::string>{lua::Stack::read<std::string>(L, 2)} : lua::Stack::read<std::vector<std::string>>(L, 2);
    const lua::Promise promise(lua::Runtime::getEngine(L));
    auto remaining = std::make_shared<std::size_t>(groups.size());
    for (const std::string& group : groups) {
        // clang-format off
        load.preload(group, [promise, remaining](const std::optional<haylen::core::AppError>& failure) {
            if (failure) {
                promise.rejectHandled(failure->what());
                return;
            }

            if (--*remaining == 0) {
                promise.resolveWith([](lua_State* state) { lua_pushboolean(state, 1); });
            }
        });
        // clang-format on
    }

    if (groups.empty()) {
        promise.resolveWith([](lua_State* state) { lua_pushboolean(state, 1); });
    }

    promise.push(L);
    return 1;
}

int SceneBinding::loadParams(lua_State* L) {
    lua::ScriptedScene::pushParams(L, lua::Userdata::check<haylen::core::SceneLoad>(L, 1).getParams());
    return 1;
}

int SceneBinding::open(lua_State* L) {
    const luaL_Reg functions[] = {
        {"push", &lua::Binding::native<&push>}, {"replace", &lua::Binding::native<&replace>}, {"pop", &lua::Binding::native<&pop>}, {"popTo", &lua::Binding::native<&popTo>}, {"popToRoot", &lua::Binding::native<&popToRoot>}, {"preload", &lua::Binding::native<&preload>}, {"cancelPreload", &lua::Binding::native<&cancelPreload>}, {"clear", &lua::Binding::native<&clear>}, {"size", &size}, {"top", &top}, {"at", &lua::Binding::native<&at>}, {"list", &list}, {"transitioning", &transitioning}, {"loadingViewOpacity", &loadingViewOpacity}, {"state", &lua::Binding::native<&state>}, {"loadProgress", &lua::Binding::native<&sceneLoadProgress>}, {"listen", &lua::Binding::native<&listen>}, {"spawn", &lua::Binding::native<&spawn>}, {nullptr, nullptr},
    };
    lua::Binding::newModule(L, functions);

    // The `Scene` base class gives scenes built with `haylen.class` the `listen` and `spawn` methods.
    ClassBinding::push(L);
    lua_pushliteral(L, "Scene");
    lua_call(L, 1, 1);
    for (const char* name : {"listen", "spawn"}) {
        lua_getfield(L, -2, name);
        lua_setfield(L, -2, name);
    }

    lua_setfield(L, -2, "Scene");
    return 1;
}

void SceneBinding::install(lua_State* L) {
    EventsBinding::addPayload<haylen::core::Scene>(&pushSceneEvent);
    EventsBinding::addPayload<haylen::core::SceneManager::Transfer>(&pushTransfer);
    EventsBinding::addPayload<haylen::core::SceneManager::LoadFailure>(&pushLoadFailure);
    lua::ClassBuilder<haylen::core::SceneLoad>(L).function("progress", &lua::Binding::native<&loadProgress>).function("preload", &lua::Binding::native<&loadPreload>).property("params", &lua::Binding::native<&loadParams>).install();
    lua::Binding::preload(L, "haylen.scene", &open);
}

} // namespace haylen::lua::core
