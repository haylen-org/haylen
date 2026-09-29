#include "lua/ScriptedLoadingView.hpp"

#include <exception>

#include "haylen/lua/Runtime.hpp"
#include "haylen/lua/Stack.hpp"
#include "lua/Owners.hpp"

namespace haylen::lua {

ScriptedLoadingView::ScriptedLoadingView(lua_State* L, int index) : table(L, index) {}

void ScriptedLoadingView::call(const char* name, const core::SceneLoad::Progress* progress, int arguments, const std::function<void(lua_State*)>& pushArguments) const {
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
        if (progress != nullptr) {
            lua_pushnumber(L, progress->value);
            Stack::push(L, progress->message);
        }
        lua_call(L, 1 + arguments + (progress != nullptr ? 2 : 0), 0);
    });
    // clang-format on
}

void ScriptedLoadingView::enter(core::Engine&) {
    call("enter", nullptr);
}

// Everything the table owns ends after its exit hook, even when the hook fails.
void ScriptedLoadingView::exit(core::Engine&) {
    std::exception_ptr failure;
    try {
        call("exit", nullptr);
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

void ScriptedLoadingView::update(core::Engine&, float deltaSeconds, const core::SceneLoad::Progress& progress) {
    call("update", &progress, 1, [deltaSeconds](lua_State* L) { lua_pushnumber(L, deltaSeconds); });
}

void ScriptedLoadingView::render(core::Engine&, const core::SceneLoad::Progress& progress) {
    call("render", &progress);
}

void ScriptedLoadingView::renderUi(core::Engine&, const core::SceneLoad::Progress& progress) {
    call("renderUi", &progress);
}

} // namespace haylen::lua
