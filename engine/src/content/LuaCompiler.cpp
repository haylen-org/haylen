#include "content/LuaCompiler.hpp"

#include <array>
#include <format>
#include <memory>
#include <stdexcept>

namespace haylen::content {

int LuaCompiler::write(lua_State*, const void* bytes, std::size_t size, void* output) {
    const auto* first = static_cast<const std::uint8_t*>(bytes);
    std::vector<std::uint8_t>& chunk = *static_cast<std::vector<std::uint8_t>*>(output);
    chunk.insert(chunk.end(), first, first + size);
    return 0;
}

std::vector<std::uint8_t> LuaCompiler::compile(std::string_view source, std::string_view path) {
    const std::unique_ptr<lua_State, decltype(&lua_close)> state(luaL_newstate(), &lua_close);
    if (state == nullptr) {
        throw std::runtime_error("Lua could not start to compile the module \"" + std::string(path) + "\".");
    }
    lua_State* L = state.get();
    const std::string name = "@" + std::string(path);
    if (luaL_loadbufferx(L, source.data(), source.size(), name.c_str(), "t") != LUA_OK) {
        throw std::invalid_argument("The Lua module \"" + std::string(path) + "\" does not compile: " + lua_tostring(L, -1) + ".");
    }

    std::vector<std::uint8_t> chunk;
    if (lua_dump(L, &write, &chunk, 0) != 0) {
        throw std::runtime_error("Lua could not write the bytecode of the module \"" + std::string(path) + "\".");
    }
    return chunk;
}

std::string LuaCompiler::getAbi() {
    static const std::string& abi = *new std::string(readAbi());
    return abi;
}

std::string LuaCompiler::readAbi() {
    // The header of a chunk follows the signature with the version, the format and six bytes of conversion checks, and then the size and a sample value of an `int`, an instruction, a Lua integer and a Lua number. The sample of the `int` is negative, so its first byte tells the byte order.
    const std::vector<std::uint8_t> chunk = compile("", "abi");
    std::size_t position = kSignature.size();
    const std::uint8_t version = chunk[position];
    const std::uint8_t format = chunk[position + 1];
    position += 2 + kConversionCheckSize;
    const bool littleEndian = chunk[position + 1] != 0xFF;
    std::array<std::size_t, 4> sizes{};
    for (std::size_t& size : sizes) {
        size = chunk[position];
        position += 1 + size;
    }
    return std::format("lua-{}.{}-f{}-i{}-x{}-l{}-n{}-{}", version >> 4, version & 0x0F, format, sizes[0], sizes[1], sizes[2], sizes[3], littleEndian ? "le" : "be");
}

} // namespace haylen::content
