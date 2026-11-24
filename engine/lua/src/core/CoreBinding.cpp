#include "core/CoreBinding.hpp"

#include <lua.hpp>

#include <array>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "core/ClassBinding.hpp"
#include "haylen/core/AppConfig.hpp"
#include "haylen/core/Engine.hpp"
#include "haylen/core/EnumNames.hpp"
#include "haylen/core/FrameClock.hpp"
#include "haylen/core/JobSystem.hpp"
#include "haylen/core/Version.hpp"
#include "haylen/graphics/Device.hpp"
#include "haylen/graphics/Viewport.hpp"
#include "haylen/io/Package.hpp"
#include "haylen/io/Path.hpp"
#include "haylen/lua/Application.hpp"
#include "haylen/lua/Binding.hpp"
#include "haylen/lua/JsonConverter.hpp"
#include "haylen/lua/Promise.hpp"
#include "haylen/lua/Runtime.hpp"
#include "haylen/lua/ScriptPlugin.hpp"
#include "haylen/lua/Stack.hpp"
#include "haylen/lua/Table.hpp"
#include "haylen/lua/TypeConverter.hpp"
#include "haylen/platform/SafeAreaSimulation.hpp"
#include "haylen/platform/Window.hpp"
#include "runtime/Autoloads.hpp"
#include "runtime/Config.hpp"

namespace haylen::core {

template <> struct EnumNames<Log::Level> {
    static constexpr std::array<std::pair<std::string_view, Log::Level>, 4> kLevels{{{"debug", Log::Level::Debug}, {"info", Log::Level::Info}, {"warning", Log::Level::Warning}, {"error", Log::Level::Error}}};

    static std::optional<Log::Level> fromName(std::string_view name) {
        for (const auto& [candidate, level] : kLevels) {
            if (candidate == name) {
                return level;
            }
        }

        return std::nullopt;
    }

    static std::string_view name(Log::Level value) {
        for (const auto& [candidate, level] : kLevels) {
            if (level == value) {
                return candidate;
            }
        }

        return kLevels.front().first;
    }
};

} // namespace haylen::core

namespace haylen::lua::core {

std::string CoreBinding::joinArguments(lua_State* L) {
    std::string line;
    const int count = lua_gettop(L);
    for (int index = 1; index <= count; ++index) {
        if (index > 1) {
            line.push_back('\t');
        }

        std::size_t length = 0;
        const char* text = luaL_tolstring(L, index, &length);
        line.append(text, length);
        lua_pop(L, 1);
    }

    return line;
}

template <haylen::core::Log::Level Level> int CoreBinding::logMessage(lua_State* L) {
    haylen::core::Log::write(Level, joinArguments(L));
    return 0;
}

int CoreBinding::openLog(lua_State* L) {
    const luaL_Reg functions[] = {
        {"debug", &logMessage<haylen::core::Log::Level::Debug>}, {"info", &logMessage<haylen::core::Log::Level::Info>}, {"warning", &logMessage<haylen::core::Log::Level::Warning>}, {"error", &logMessage<haylen::core::Log::Level::Error>}, {"level", &lua::Binding::function<&haylen::core::Log::getLevel>}, {"setLevel", &lua::Binding::function<&haylen::core::Log::setLevel>}, {nullptr, nullptr},
    };
    lua::Binding::newModule(L, functions);
    return 1;
}

// Stops the app with `haylen.quit(options)`, whose `status` option is the exit status of the process, 0 by default.
int CoreBinding::rootQuit(lua_State* L) {
    std::uint8_t status = 0;
    if (!lua_isnoneornil(L, 1)) {
        luaL_checktype(L, 1, LUA_TTABLE);
        lua::Table::checkFields(L, 1, {kQuitFields});
        lua::Table::readField(L, 1, "status", status);
    }

    lua::Runtime::getEngine(L).quit(status);
    return 0;
}

// Runs the app package at a path of the file system with `haylen.launch(path, options)` once this frame ends, in development when the option `developmentServer` names the server that sends its saved files, without native code when `nativeAccess` is false, and the app that launched it starts again when it quits.
int CoreBinding::rootLaunch(lua_State* L) {
    haylen::core::Engine::Launch launch{.package = lua::Stack::read<std::string_view>(L, 1)};
    if (!lua_isnoneornil(L, 2)) {
        luaL_checktype(L, 2, LUA_TTABLE);
        lua::Table::checkFields(L, 2, {kLaunchFields});
        lua::Table::readField(L, 2, "developmentServer", launch.developmentServer);
        lua::Table::readField(L, 2, "nativeAccess", launch.nativeAccess);
    }

    lua::Runtime::getEngine(L).requestLaunch(std::move(launch));
    return 0;
}

// Opens the package and reads its `haylen.json` and the logo of its splash, which a package without one leaves empty. A package that cannot open fails with the reason.
CoreBinding::PackageInfo CoreBinding::readPackageInfo(const std::filesystem::path& path) {
    std::error_code error;
    if (!std::filesystem::exists(path, error)) {
        throw std::invalid_argument("No app package is at \"" + path.string() + "\". Name a folder or a zip file that holds \"haylen.json\".");
    }

    const std::unique_ptr<haylen::io::Package> package = haylen::io::Package::open(path);
    const haylen::core::AppConfig config = haylen::core::AppConfig::fromPackage(*package, lua::Application::kSections);
    PackageInfo info{.name = config.name, .identifier = config.identifier, .version = config.version, .build = config.build, .logo = config.splash.logo};
    if (!info.logo.empty()) {
        info.logoBytes = package->read(haylen::io::Path::asset(info.logo));
    }

    return info;
}

void CoreBinding::pushPackageInfo(lua_State* L, const PackageInfo& info) {
    lua_createtable(L, 0, 6);
    lua::Stack::push(L, info.name);
    lua_setfield(L, -2, "name");
    lua::Stack::push(L, info.identifier);
    lua_setfield(L, -2, "identifier");
    lua::Stack::push(L, info.version);
    lua_setfield(L, -2, "version");
    lua_pushinteger(L, info.build);
    lua_setfield(L, -2, "build");

    if (info.logo.empty()) {
        return;
    }

    lua::Stack::push(L, info.logo);
    lua_setfield(L, -2, "logo");
    lua_pushlstring(L, reinterpret_cast<const char*>(info.logoBytes.data()), info.logoBytes.size());
    lua_setfield(L, -2, "logoBytes");
}

// Reads what an app package says about itself with `haylen.packageInfo(path)` on the I/O pool, such as the name and the logo that a list of downloaded apps shows, and resolves on a later frame.
int CoreBinding::rootPackageInfo(lua_State* L) {
    haylen::core::Engine& engine = lua::Runtime::getEngine(L);
    std::filesystem::path path(lua::Stack::read<std::string_view>(L, 1));
    const lua::Promise promise(engine);
    // clang-format off
    engine.getJobs().runIo([path = std::move(path)] { return readPackageInfo(path); }, [promise](haylen::core::JobSystem::Result<PackageInfo> result) {
        if (!result.isOk()) {
            result.error = "The app package could not be read. " + result.error;
        }

        promise.settle(std::move(result), &pushPackageInfo);
    });
    // clang-format on

    promise.push(L);
    return 1;
}

int CoreBinding::rootLaunched(lua_State* L) {
    lua::Stack::push(L, lua::Runtime::getEngine(L).isLaunched());
    return 1;
}

// Returns the launch arguments of the app as a new list of strings.
int CoreBinding::rootLaunchArguments(lua_State* L) {
    const std::span<const std::string> arguments = lua::Runtime::getEngine(L).getLaunchArguments();
    lua_createtable(L, static_cast<int>(arguments.size()), 0);
    for (std::size_t index = 0; index < arguments.size(); ++index) {
        lua::Stack::push(L, arguments[index]);
        lua_rawseti(L, -2, static_cast<lua_Integer>(index) + 1);
    }

    return 1;
}

int CoreBinding::rootCanQuit(lua_State* L) {
    lua::Stack::push(L, lua::Runtime::getEngine(L).getWindow().canQuit());
    return 1;
}

// Starts the app again from its package once the frame ends, the way a hot reload does.
int CoreBinding::rootRequestRestart(lua_State* L) {
    lua::Runtime::getEngine(L).requestRestart();
    return 0;
}

int CoreBinding::rootElapsed(lua_State* L) {
    lua::Stack::push(L, lua::Runtime::getEngine(L).getClock().getElapsed());
    return 1;
}

int CoreBinding::rootDelta(lua_State* L) {
    lua::Stack::push(L, lua::Runtime::getEngine(L).getClock().getDelta());
    return 1;
}

int CoreBinding::rootUnscaledDelta(lua_State* L) {
    lua::Stack::push(L, lua::Runtime::getEngine(L).getClock().getUnscaledDelta());
    return 1;
}

int CoreBinding::rootFrameIndex(lua_State* L) {
    lua::Stack::push(L, lua::Runtime::getEngine(L).getClock().getFrameIndex());
    return 1;
}

int CoreBinding::rootTimeScale(lua_State* L) {
    lua::Stack::push(L, lua::Runtime::getEngine(L).getClock().getTimeScale());
    return 1;
}

int CoreBinding::rootSetTimeScale(lua_State* L) {
    lua::Runtime::getEngine(L).getClock().setTimeScale(lua::Stack::read<double>(L, 1));
    return 0;
}

int CoreBinding::rootFixedStep(lua_State* L) {
    lua::Stack::push(L, lua::Runtime::getEngine(L).getClock().getFixedStep());
    return 1;
}

int CoreBinding::rootInterpolation(lua_State* L) {
    lua::Stack::push(L, lua::Runtime::getEngine(L).getClock().getInterpolation());
    return 1;
}

// Stops the app with the message and the stack of the Lua code that reported it.
int CoreBinding::rootReportError(lua_State* L) {
    lua::Runtime::getEngine(L).reportError(lua::Runtime::captureError(L, lua::Stack::read<std::string>(L, 1), 1));
    return 0;
}

int CoreBinding::rootRecoverable(lua_State* L) {
    lua::Stack::push(L, lua::Runtime::getEngine(L).isRecoverable());
    return 1;
}

int CoreBinding::rootSetRecoverable(lua_State* L) {
    lua::Runtime::getEngine(L).setRecoverable(lua::Stack::read<bool>(L, 1));
    return 0;
}

int CoreBinding::rootRecover(lua_State* L) {
    lua::Runtime::getEngine(L).recover();
    return 0;
}

int CoreBinding::rootPaused(lua_State* L) {
    lua::Stack::push(L, lua::Runtime::getEngine(L).isPaused());
    return 1;
}

int CoreBinding::rootSetPaused(lua_State* L) {
    lua::Runtime::getEngine(L).setPaused(lua::Stack::read<bool>(L, 1));
    return 0;
}

int CoreBinding::rootAppState(lua_State* L) {
    const haylen::core::Engine::AppState state = lua::Runtime::getEngine(L).getAppState();
    lua::Stack::push(L, haylen::core::Engine::kAppStateNames[static_cast<std::size_t>(state)]);
    return 1;
}

int CoreBinding::rootAppCovered(lua_State* L) {
    lua::Stack::push(L, lua::Runtime::getEngine(L).isAppCovered());
    return 1;
}

int CoreBinding::rootNetworkState(lua_State* L) {
    const haylen::core::Engine::NetworkState state = lua::Runtime::getEngine(L).getNetworkState();
    lua::Stack::push(L, haylen::core::Engine::kNetworkStateNames[static_cast<std::size_t>(state)]);
    return 1;
}

int CoreBinding::rootHalted(lua_State* L) {
    lua::Stack::push(L, lua::Runtime::getEngine(L).isHalted());
    return 1;
}

int CoreBinding::rootDrawing(lua_State* L) {
    lua::Stack::push(L, lua::Runtime::getEngine(L).isDrawing());
    return 1;
}

int CoreBinding::rootLifecycle(lua_State* L) {
    const haylen::core::AppConfig::Lifecycle& lifecycle = lua::Runtime::getEngine(L).getLifecycle();
    lua_createtable(L, 0, 3);
    lua::Stack::push(L, lifecycle.pauseOnBackground);
    lua_setfield(L, -2, "pauseOnBackground");
    lua::Stack::push(L, lifecycle.pauseOnFocusLoss);
    lua_setfield(L, -2, "pauseOnFocusLoss");
    lua::Stack::push(L, lifecycle.muteOnFocusLoss);
    lua_setfield(L, -2, "muteOnFocusLoss");
    return 1;
}

// Changes the lifecycle options that the table names and keeps the others.
int CoreBinding::rootSetLifecycle(lua_State* L) {
    luaL_checktype(L, 1, LUA_TTABLE);
    lua::Table::checkFields(L, 1, {kLifecycleFields});
    haylen::core::Engine& engine = lua::Runtime::getEngine(L);
    haylen::core::AppConfig::Lifecycle lifecycle = engine.getLifecycle();
    lua::Table::readField(L, 1, "pauseOnBackground", lifecycle.pauseOnBackground);
    lua::Table::readField(L, 1, "pauseOnFocusLoss", lifecycle.pauseOnFocusLoss);
    lua::Table::readField(L, 1, "muteOnFocusLoss", lifecycle.muteOnFocusLoss);
    engine.setLifecycle(lifecycle);
    return 0;
}

// Loads a module as an autoload with `haylen.autoload(name, module)` and returns its table. The name defaults to the last part of the module in camel case.
int CoreBinding::rootAutoload(lua_State* L) {
    const std::string module = lua::Stack::read<std::string>(L, lua_isnoneornil(L, 2) ? 1 : 2);
    const std::string name = lua_isnoneornil(L, 2) ? lua::Autoloads::getName(module) : lua::Stack::read<std::string>(L, 1);
    lua::Runtime::getEngine(L).getPlugin<lua::ScriptPlugin>().getAutoloads().add(L, name, module);
    lua::Autoloads::pushTable(L);
    lua_getfield(L, -1, name.c_str());
    return 1;
}

int CoreBinding::openRoot(lua_State* L) {
    haylen::core::Engine& owner = lua::Runtime::getEngine(L);
    const luaL_Reg functions[] = {
        {"quit", &rootQuit}, {"canQuit", &rootCanQuit}, {"launch", &lua::Binding::native<&rootLaunch>}, {"launched", &rootLaunched}, {"launchArguments", &rootLaunchArguments}, {"packageInfo", &lua::Binding::native<&rootPackageInfo>}, {"requestRestart", &rootRequestRestart}, {"elapsed", &rootElapsed}, {"delta", &rootDelta}, {"unscaledDelta", &rootUnscaledDelta}, {"frameIndex", &rootFrameIndex}, {"timeScale", &rootTimeScale}, {"setTimeScale", &lua::Binding::native<&rootSetTimeScale>}, {"fixedStep", &rootFixedStep}, {"interpolation", &rootInterpolation}, {"reportError", &lua::Binding::native<&rootReportError>}, {"recoverable", &rootRecoverable}, {"setRecoverable", &lua::Binding::native<&rootSetRecoverable>}, {"recover", &rootRecover}, {"paused", &rootPaused}, {"setPaused", &lua::Binding::native<&rootSetPaused>}, {"appState", &rootAppState}, {"appCovered", &rootAppCovered}, {"networkState", &rootNetworkState}, {"halted", &rootHalted}, {"drawing", &rootDrawing}, {"lifecycle", &rootLifecycle}, {"setLifecycle", &lua::Binding::native<&rootSetLifecycle>}, {"autoload", &lua::Binding::native<&rootAutoload>}, {nullptr, nullptr},
    };
    lua::Binding::newModule(L, functions);
    ClassBinding::push(L);
    lua_setfield(L, -2, "class");
    lua::Autoloads::pushTable(L);
    lua_setfield(L, -2, "autoloads");
    lua::Stack::push(L, haylen::core::Version::kString);
    lua_setfield(L, -2, "version");
    lua::Stack::push(L, owner.getPlatformName());
    lua_setfield(L, -2, "platform");
    lua::Stack::push(L, owner.getGraphics().getApiName());
    lua_setfield(L, -2, "graphicsApi");
    haylen::core::Json config = owner.getConfig().toJson();
    config[lua::Config::kSection] = owner.getPlugin<lua::ScriptPlugin>().getConfig().toJson();
    lua::JsonConverter::push(L, config);
    lua_setfield(L, -2, "config");
    return 1;
}

int CoreBinding::viewportDesignSize(lua_State* L) {
    const haylen::math::Vec2 size = lua::Runtime::getEngine(L).getViewport().getDesignSize();
    lua::Stack::push(L, size.x);
    lua::Stack::push(L, size.y);
    return 2;
}

int CoreBinding::viewportVisibleRect(lua_State* L) {
    lua::Stack::push(L, lua::Runtime::getEngine(L).getViewport().getVisibleRect());
    return 1;
}

int CoreBinding::viewportSafeRect(lua_State* L) {
    lua::Stack::push(L, lua::Runtime::getEngine(L).getViewport().getSafeRect());
    return 1;
}

// Returns the edges that native views reserve as a table of design units, which the safe rectangle already leaves out.
int CoreBinding::viewportReservedInsets(lua_State* L) {
    haylen::core::Engine& engine = lua::Runtime::getEngine(L);
    const haylen::math::Insets& pixels = engine.getReservedInsets();
    const haylen::math::Vec2 scale = engine.getViewport().getPixelsPerUnit();
    lua::Stack::push(L, haylen::math::Insets{.left = pixels.left / scale.x, .top = pixels.top / scale.y, .right = pixels.right / scale.x, .bottom = pixels.bottom / scale.y});
    return 1;
}

// Returns the cutouts of the screen over the window as a list of rectangles of design units.
int CoreBinding::viewportCutouts(lua_State* L) {
    haylen::core::Engine& engine = lua::Runtime::getEngine(L);
    const haylen::graphics::Viewport& viewport = engine.getViewport();
    const std::span<const haylen::math::Rect> cutouts = engine.getCutouts();
    lua_createtable(L, static_cast<int>(cutouts.size()), 0);
    for (std::size_t index = 0; index < cutouts.size(); ++index) {
        lua::Stack::push(L, haylen::math::Rect::fromMinMax(viewport.toDesign(cutouts[index].getMin()), viewport.toDesign(cutouts[index].getMax())));
        lua_rawseti(L, -2, static_cast<lua_Integer>(index) + 1);
    }

    return 1;
}

int CoreBinding::viewportPixelRect(lua_State* L) {
    lua::Stack::push(L, lua::Runtime::getEngine(L).getViewport().getPixelRect());
    return 1;
}

int CoreBinding::viewportPixelsPerUnit(lua_State* L) {
    const haylen::math::Vec2 scale = lua::Runtime::getEngine(L).getViewport().getPixelsPerUnit();
    lua::Stack::push(L, scale.x);
    lua::Stack::push(L, scale.y);
    return 2;
}

int CoreBinding::viewportToDesign(lua_State* L) {
    const haylen::math::Vec2 point = lua::Runtime::getEngine(L).getViewport().toDesign({lua::Stack::read<float>(L, 1), lua::Stack::read<float>(L, 2)});
    lua::Stack::push(L, point.x);
    lua::Stack::push(L, point.y);
    return 2;
}

int CoreBinding::viewportToFramebuffer(lua_State* L) {
    const haylen::math::Vec2 point = lua::Runtime::getEngine(L).getViewport().toFramebuffer({lua::Stack::read<float>(L, 1), lua::Stack::read<float>(L, 2)});
    lua::Stack::push(L, point.x);
    lua::Stack::push(L, point.y);
    return 2;
}

int CoreBinding::viewportScaling(lua_State* L) {
    lua::Stack::push(L, lua::Runtime::getEngine(L).getViewport().getScaling());
    return 1;
}

int CoreBinding::viewportSetScaling(lua_State* L) {
    lua::Runtime::getEngine(L).setScaling(lua::Stack::read<haylen::graphics::Viewport::ScalingPolicy>(L, 1));
    return 0;
}

int CoreBinding::viewportSetDesignSize(lua_State* L) {
    lua::Runtime::getEngine(L).setDesignSize({lua::Stack::read<float>(L, 1), lua::Stack::read<float>(L, 2)});
    return 0;
}

// Returns the simulated safe area as a device name or insets from the top clockwise, or `nil` while the device reports its own.
int CoreBinding::viewportSafeAreaSimulation(lua_State* L) {
    const std::optional<haylen::platform::SafeAreaSimulation>& simulation = lua::Runtime::getEngine(L).getSafeAreaSimulation();
    if (!simulation) {
        lua_pushnil(L);
        return 1;
    }

    lua::JsonConverter::push(L, simulation->toJson());
    return 1;
}

// Simulates the safe area of a device by name or of insets in window points with `setSafeAreaSimulation(value)`, and `nil` goes back to the safe area of the device.
int CoreBinding::viewportSetSafeAreaSimulation(lua_State* L) {
    haylen::core::Engine& engine = lua::Runtime::getEngine(L);
    if (lua_isnoneornil(L, 1)) {
        engine.setSafeAreaSimulation(std::nullopt);
        return 0;
    }

    engine.setSafeAreaSimulation(haylen::platform::SafeAreaSimulation::fromJson(lua::JsonConverter::read(L, 1)));
    return 0;
}

int CoreBinding::openViewport(lua_State* L) {
    const luaL_Reg functions[] = {
        {"designSize", &viewportDesignSize}, {"visibleRect", &viewportVisibleRect}, {"safeRect", &viewportSafeRect}, {"reservedInsets", &viewportReservedInsets}, {"cutouts", &viewportCutouts}, {"pixelRect", &viewportPixelRect}, {"pixelsPerUnit", &viewportPixelsPerUnit}, {"toDesign", &viewportToDesign}, {"toFramebuffer", &viewportToFramebuffer}, {"scaling", &viewportScaling}, {"setScaling", &viewportSetScaling}, {"setDesignSize", &lua::Binding::native<&viewportSetDesignSize>}, {"safeAreaSimulation", &viewportSafeAreaSimulation}, {"setSafeAreaSimulation", &lua::Binding::native<&viewportSetSafeAreaSimulation>}, {nullptr, nullptr},
    };
    lua::Binding::newModule(L, functions);
    return 1;
}

void CoreBinding::install(lua_State* L) {
    lua::Binding::preload(L, "haylen", &lua::Binding::native<&openRoot>);
    lua::Binding::preload(L, "haylen.log", &openLog);
    lua::Binding::preload(L, "haylen.viewport", &openViewport);
}

} // namespace haylen::lua::core
