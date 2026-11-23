#pragma once

#include <cstdint>

namespace haylen::spatial2d {

// A spatial structure created from Lua. Lua keys entries by any value, usually the entity table itself, and only the binding knows the ids the structure sees.
template <typename Structure> struct ScriptedIndex {
    Structure index;
    std::uint64_t nextId = 1;
};

} // namespace haylen::spatial2d
