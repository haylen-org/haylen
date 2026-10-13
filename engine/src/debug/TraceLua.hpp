#pragma once

#include <array>
#include <string_view>

struct lua_State;

namespace haylen::debug {

// Adds the trace functions to `haylen.debug`: starting a trace of the profiler, the workers and the GPU, and writing it to a file of user storage off the frame thread.
class TraceLua final {
  public:
    static void addFunctions(lua_State* L);

  private:
    static constexpr std::array<std::string_view, 2> kTraceFields{"frames", "capacity"};

    static int startTrace(lua_State* L);
    static int tracing(lua_State* L);
    static int stopTraceAsync(lua_State* L);
};

} // namespace haylen::debug
