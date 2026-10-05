#include "haylen/lua/Promise.hpp"

#include <memory>
#include <stdexcept>
#include <utility>

#include "haylen/core/Engine.hpp"
#include "haylen/lua/JsonConverter.hpp"
#include "varn/async/Promise.h"

namespace haylen::lua {

Promise::Promise(core::Engine& engine) : promise(std::make_shared<varn::async::Promise>(engine.getScriptRuntime())) {}

bool Promise::isPromise(lua_State* L, int index) {
    return luaL_testudata(L, index, kVarnType) != nullptr;
}

void Promise::push(lua_State* L) const {
    varn::async::Promise::push(L, promise);
}

// A value Lua cannot hold would fail while the awaiting coroutines resume and leave them waiting forever, so it rejects the promise instead.
void Promise::resolve(core::Json value) const {
    try {
        JsonConverter::validate(value);
    } catch (const std::invalid_argument& error) {
        reject(error.what());
        return;
    }
    // Varn copies the function every time a coroutine awaits the promise, so the value is shared instead of copied each time.
    promise->resolveCustom([value = std::make_shared<const core::Json>(std::move(value))](lua_State* L) { JsonConverter::push(L, *value); });
}

void Promise::resolveWith(std::function<void(lua_State* L)> pushValue) const {
    promise->resolveCustom(std::move(pushValue));
}

void Promise::reject(std::string message) const {
    promise->reject(std::move(message));
}

bool Promise::isSettled() const {
    return promise->state() != varn::async::Promise::State::Pending;
}

} // namespace haylen::lua
