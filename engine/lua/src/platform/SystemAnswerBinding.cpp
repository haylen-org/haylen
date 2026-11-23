#include "platform/SystemAnswerBinding.hpp"

#include <lua.hpp>

#include <utility>

#include "haylen/core/Engine.hpp"
#include "haylen/core/Json.hpp"
#include "haylen/core/Log.hpp"
#include "haylen/lua/JsonConverter.hpp"
#include "haylen/lua/Promise.hpp"
#include "haylen/lua/ScriptPlugin.hpp"
#include "haylen/lua/Stack.hpp"

namespace haylen::lua::platform {

std::unordered_set<std::string> SystemAnswerBinding::loggedRefusals;

void SystemAnswerBinding::settle(haylen::core::Engine& engine, const lua::Promise& promise, const haylen::platform::SystemAnswer& answer, std::optional<std::string> value) {
    if (answer.state == haylen::platform::SystemAnswer::State::Granted) {
        promise.resolve(value ? haylen::core::Json(std::move(*value)) : haylen::core::Json());
        return;
    }

    if (loggedRefusals.insert(answer.message).second) {
        haylen::core::Log::warning("{}", answer.message);
    }

    reject(engine, promise, answer);
}

void SystemAnswerBinding::reject(haylen::core::Engine& engine, const lua::Promise& promise, const haylen::platform::SystemAnswer& answer) {
    lua_State* main = engine.getPlugin<lua::ScriptPlugin>().getState();
    lua_createtable(main, 0, 3);
    lua::Stack::push(main, answer.message);
    lua_setfield(main, -2, "error");
    lua::Stack::push(main, answer.state == haylen::platform::SystemAnswer::State::Denied ? "denied" : "unsupported");
    lua_setfield(main, -2, "code");

    if (!answer.missing.empty()) {
        lua_createtable(main, 0, 1);
        lua::JsonConverter::push(main, answer.missingToJson());
        lua_setfield(main, -2, "missing");
        lua_setfield(main, -2, "data");
    }

    promise.rejectHandledFailure(main);
}

} // namespace haylen::lua::platform
