#include "net/MessagePackLua.hpp"

#include <lua.hpp>

#include <cstdint>
#include <cstring>
#include <exception>
#include <optional>
#include <span>
#include <string_view>
#include <utility>

#include "haylen/core/Engine.hpp"
#include "haylen/core/JobSystem.hpp"
#include "haylen/core/JsonBytes.hpp"
#include "haylen/core/MessagePack.hpp"
#include "haylen/lua/Binding.hpp"
#include "haylen/lua/JsonConverter.hpp"
#include "haylen/lua/Promise.hpp"
#include "haylen/lua/Runtime.hpp"
#include "haylen/lua/Stack.hpp"

namespace haylen::net {

void MessagePackLua::embedBytes(core::Json& value, std::vector<std::vector<std::byte>>& buffers) {
    if (const std::optional<std::size_t> reference = core::JsonBytes::findReference(value)) {
        const std::vector<std::byte>& bytes = buffers.at(*reference);
        core::Json::binary_t binary;
        binary.reserve(bytes.size());
        for (const std::byte byte : bytes) {
            binary.push_back(static_cast<std::uint8_t>(byte));
        }
        value = core::Json::binary(std::move(binary));
        return;
    }
    if (value.is_structured()) {
        for (core::Json& element : value) {
            embedBytes(element, buffers);
        }
    }
}

// Encodes a Lua value with `encodeMessagePack(value)` into a string of bytes, where a `haylen.Bytes` value becomes a binary value.
int MessagePackLua::encodeMessagePack(lua_State* L) {
    std::vector<std::vector<std::byte>> buffers;
    core::Json value = lua::JsonConverter::read(L, 1, buffers);
    embedBytes(value, buffers);
    const std::vector<std::byte> encoded = core::MessagePack::encode(value);
    lua_pushlstring(L, reinterpret_cast<const char*>(encoded.data()), encoded.size());
    return 1;
}

int MessagePackLua::decodeMessagePack(lua_State* L) {
    const std::string_view bytes = lua::Stack::read<std::string_view>(L, 1);
    lua::JsonConverter::push(L, core::MessagePack::decode(std::as_bytes(std::span(bytes.data(), bytes.size()))));
    return 1;
}

// The bytes are copied once, and the value that the task pool builds reaches Lua on the frame thread.
int MessagePackLua::decodeMessagePackAsync(lua_State* L) {
    const std::string_view text = lua::Stack::read<std::string_view>(L, 1);
    core::Engine& engine = lua::Runtime::getEngine(L);
    const lua::Promise promise(engine);
    std::vector<std::byte> bytes(text.size());
    std::memcpy(bytes.data(), text.data(), text.size());
    // clang-format off
    engine.getJobs().post([promise, bytes = std::move(bytes)] {
        try {
            core::Json value = core::MessagePack::decode(bytes);
            lua::JsonConverter::validate(value);
            promise.resolve(std::move(value));
        } catch (const std::exception& error) {
            promise.reject(error.what());
        }
    });
    // clang-format on
    promise.push(L);
    return 1;
}

void MessagePackLua::addFunctions(lua_State* L) {
    const luaL_Reg functions[] = {
        {"encodeMessagePack", &lua::Binding::native<&encodeMessagePack>},
        {"decodeMessagePack", &lua::Binding::native<&decodeMessagePack>},
        {"decodeMessagePackAsync", &lua::Binding::native<&decodeMessagePackAsync>},
        {nullptr, nullptr},
    };
    luaL_setfuncs(L, functions, 0);
}

} // namespace haylen::net
