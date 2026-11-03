#include "platform/AnswerLua.hpp"

#include <lua.hpp>

#include <utility>

#include "haylen/core/Engine.hpp"
#include "haylen/core/Json.hpp"
#include "haylen/core/Log.hpp"
#include "haylen/lua/JsonConverter.hpp"
#include "haylen/lua/Promise.hpp"
#include "haylen/lua/Stack.hpp"

namespace haylen::platform {

std::unordered_set<std::string> AnswerLua::loggedRefusals;

void AnswerLua::settlePermission(core::Engine& engine, const lua::Promise& promise, const Permission::Answer& answer) {
    if (answer.state != Permission::State::Unsupported) {
        promise.resolve(std::string(Permission::stateName(answer.state)));
        return;
    }
    reject(engine, promise, answer);
}

void AnswerLua::settle(core::Engine& engine, const lua::Promise& promise, const Permission::Answer& answer, std::optional<std::string> value) {
    if (answer.state == Permission::State::Granted) {
        promise.resolve(value ? core::Json(std::move(*value)) : core::Json());
        return;
    }
    if (loggedRefusals.insert(answer.message).second) {
        core::Log::warning("{}", answer.message);
    }
    reject(engine, promise, answer);
}

void AnswerLua::reject(core::Engine& engine, const lua::Promise& promise, const Permission::Answer& answer) {
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

} // namespace haylen::platform
