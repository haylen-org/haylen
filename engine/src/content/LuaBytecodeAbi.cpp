#include "content/LuaBytecodeAbi.hpp"

#include <array>
#include <cstdint>
#include <format>
#include <memory>
#include <stdexcept>
#include <vector>

namespace haylen::content {

std::string LuaBytecodeAbi::get() {
    static const std::string& abi = *new std::string(read());
    return abi;
}

int LuaBytecodeAbi::append(lua_State*, const void* bytes, std::size_t size, void* output) {
    const auto* first = static_cast<const std::uint8_t*>(bytes);
    std::vector<std::uint8_t>& chunk = *static_cast<std::vector<std::uint8_t>*>(output);
    chunk.insert(chunk.end(), first, first + size);
    return 0;
}

std::string LuaBytecodeAbi::read() {
    const std::unique_ptr<lua_State, decltype(&lua_close)> state(luaL_newstate(), &lua_close);
    std::vector<std::uint8_t> chunk;
    if (state == nullptr || luaL_loadbufferx(state.get(), "", 0, "=abi", "t") != LUA_OK || lua_dump(state.get(), &append, &chunk, 0) != 0) {
        throw std::runtime_error("Lua could not write a chunk to name the format of its bytecode.");
    }

    // The header of a chunk follows the signature with the version, the format and six bytes of conversion checks, and then the size and a sample value of an `int`, an instruction, a Lua integer and a Lua number. The sample of the `int` is negative, so its first byte tells the byte order.
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
