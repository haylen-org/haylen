#pragma once

#include <array>
#include <string>
#include <string_view>

struct lua_State;

namespace haylen::lua {
class Promise;
}

namespace haylen::debug {

// Adds the capture of GPU frames to `haylen.debug`: the capture tool of the graphics API, and a capture of the next frames into a file of user storage that a promise resolves with.
class GpuCaptureLua final {
  public:
    static void addFunctions(lua_State* L);

  private:
    static constexpr std::array<std::string_view, 1> kCaptureFields{"frames"};

    static int gpuCaptureTool(lua_State* L);
    static int captureGpuAsync(lua_State* L);

    // Rejects with `{error, code}`, handled when the engine logs the failure too.
    static void reject(lua_State* L, const lua::Promise& promise, std::string_view code, const std::string& message, bool handled);
};

} // namespace haylen::debug
