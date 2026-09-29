#include "haylen/lua/Promise.hpp"

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

void Promise::resolve(core::Json value) const {
    promise->resolveCustom([value = std::move(value)](lua_State* L) { JsonConverter::push(L, value); });
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
