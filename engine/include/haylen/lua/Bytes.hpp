#pragma once

#include <cstddef>
#include <vector>

#include "haylen/lua/Type.hpp"

namespace haylen::lua {

// A Lua string marked to cross the platform bridge as a byte buffer, which haylen.platform.bytes makes, so text strings always stay text.
struct Bytes {
    std::vector<std::byte> data;
};

template <> struct Type<Bytes> {
    static constexpr const char* name = "haylen.Bytes";
    using Storage = Bytes;
};

} // namespace haylen::lua
