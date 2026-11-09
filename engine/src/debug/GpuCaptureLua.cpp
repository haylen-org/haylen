#include "debug/GpuCaptureLua.hpp"

#include <lua.hpp>

#include <cstddef>
#include <filesystem>
#include <optional>
#include <stdexcept>

#include "haylen/core/Engine.hpp"
#include "haylen/gpu/Device.hpp"
#include "haylen/graphics/Device.hpp"
#include "haylen/lua/Binding.hpp"
#include "haylen/lua/Promise.hpp"
#include "haylen/lua/Runtime.hpp"
#include "haylen/lua/Stack.hpp"
#include "haylen/lua/Table.hpp"
#include "haylen/plugins/DebugPlugin.hpp"

namespace haylen::debug {

int GpuCaptureLua::gpuCaptureTool(lua_State* L) {
    const gpu::Device::CaptureTool tool = lua::Runtime::getEngine(L).getGraphics().getCaptureSupport().tool;
    lua::Stack::push(L, gpu::Device::kCaptureToolNames[static_cast<std::size_t>(tool)].first);
    return 1;
}

// Captures with `captureGpuAsync({frames})`, resolving with `{path, tool, frames}` once the last frame was submitted. A device without a capture tool and a capture that records already reject at once, and a tool that fails rejects once the capture ends.
int GpuCaptureLua::captureGpuAsync(lua_State* L) {
    lua_Integer frames = 1;
    if (!lua_isnoneornil(L, 1)) {
        luaL_checktype(L, 1, LUA_TTABLE);
        lua::Table::checkFields(L, 1, {kCaptureFields});
        if (lua_getfield(L, 1, "frames") != LUA_TNIL) {
            frames = luaL_checkinteger(L, -1);
        }
        lua_pop(L, 1);
    }
    if (frames < 1) {
        throw std::invalid_argument("A GPU capture records at least 1 frame.");
    }

    core::Engine& engine = lua::Runtime::getEngine(L);
    graphics::Device& graphics = engine.getGraphics();
    const lua::Promise promise(engine);
    const gpu::Device::CaptureSupport support = graphics.getCaptureSupport();
    if (support.tool == gpu::Device::CaptureTool::None) {
        reject(L, promise, "unsupported", support.missing, false);
        promise.push(L);
        return 1;
    }
    if (graphics.isCapturing()) {
        reject(L, promise, "busy", "A GPU capture is already recording. Wait for it to end before capturing again.", false);
        promise.push(L);
        return 1;
    }

    lua_State* main = lua::Runtime::getMainThread(L);
    const std::string_view tool = gpu::Device::kCaptureToolNames[static_cast<std::size_t>(support.tool)].first;
    const int count = static_cast<int>(frames);
    std::optional<std::string> refusal;
    try {
        // clang-format off
        engine.getPlugin<plugins::DebugPlugin>().captureGpu(count, [promise, main, tool, count](std::optional<std::filesystem::path> path, const std::string& error) {
            if (!path) {
                reject(main, promise, "failed", error, true);
                return;
            }
            const std::u8string text = path->u8string();
            promise.resolve({{"path", std::string(text.begin(), text.end())}, {"tool", tool}, {"frames", count}});
        });
        // clang-format on
    } catch (const std::runtime_error& error) {
        refusal = error.what();
    }
    if (refusal) {
        reject(L, promise, "failed", *refusal, false);
    }
    promise.push(L);
    return 1;
}

void GpuCaptureLua::reject(lua_State* L, const lua::Promise& promise, std::string_view code, const std::string& message, bool handled) {
    lua_createtable(L, 0, 2);
    lua::Stack::push(L, message);
    lua_setfield(L, -2, "error");
    lua::Stack::push(L, code);
    lua_setfield(L, -2, "code");
    if (handled) {
        promise.rejectHandledFailure(L);
        return;
    }
    promise.rejectFailure(L);
}

void GpuCaptureLua::addFunctions(lua_State* L) {
    const luaL_Reg functions[] = {
        {"gpuCaptureTool", &lua::Binding::native<&gpuCaptureTool>},
        {"captureGpuAsync", &lua::Binding::native<&captureGpuAsync>},
        {nullptr, nullptr},
    };
    luaL_setfuncs(L, functions, 0);
}

} // namespace haylen::debug
