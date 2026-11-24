#pragma once

#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "haylen/core/Json.hpp"

namespace haylen::core {
struct AppConfig;
}

namespace haylen::lua {

// The settings of the Lua of an app, from the `lua` section of `haylen.json`, which the core keeps as it is for the Lua application to read: the autoloads, how a changed module applies in development and the execution budget of development.
struct Config {
    // A changed module reloads in place with the state of the app, or restarts the app. Changes that cannot apply in place restart it either way.
    enum class Reload : std::uint8_t {
        Module,
        Restart,
    };

    // Lua modules that load before the first scene and live for the whole app.
    std::vector<std::string> autoloads;

    Reload reload = Reload::Module;

    // The time that the code of an app in development may run in each entry of the host, such as one frame, before the engine cancels the app.
    std::chrono::milliseconds executionBudget{5000};

    // The bytes that the Lua state of an app in development may hold before the engine cancels the app.
    std::size_t memoryBudget = std::size_t{1024} * 1024 * 1024;

    // The section of `haylen.json` that holds the settings.
    static constexpr std::string_view kSection = "lua";

    // The bounds of `lua.executionBudget` in seconds.
    static constexpr double kMinExecutionBudget = 0.1;
    static constexpr double kMaxExecutionBudget = 600.0;

    // The bounds of `lua.memoryBudget` in megabytes.
    static constexpr double kMinMemoryBudget = 16.0;
    static constexpr double kMaxMemoryBudget = 4096.0;

    // Reads every present field of the section and validates it. Throws `std::invalid_argument` with the offending field on bad values.
    [[nodiscard]] static Config fromJson(const haylen::core::Json& section);

    // Reads the section that the configuration of the app kept, or the defaults when `haylen.json` has none.
    [[nodiscard]] static Config fromConfig(const haylen::core::AppConfig& config);
    [[nodiscard]] haylen::core::Json toJson() const;

    [[nodiscard]] static std::optional<Reload> reloadFromName(std::string_view name) noexcept;
    [[nodiscard]] static std::string_view reloadName(Reload value) noexcept;

  private:
    static const std::array<std::pair<std::string_view, Reload>, 2> kReloadNames;

    [[nodiscard]] static bool isWithin(const haylen::core::Json& value, double minimum, double maximum) noexcept;
};

} // namespace haylen::lua
