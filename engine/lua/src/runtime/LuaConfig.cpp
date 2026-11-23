#include "runtime/LuaConfig.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>

#include "haylen/core/AppConfig.hpp"
#include "haylen/core/JsonValidator.hpp"

namespace haylen::lua {

const std::array<std::pair<std::string_view, LuaConfig::Reload>, 2> LuaConfig::kReloadNames{{{"module", Reload::Module}, {"restart", Reload::Restart}}};

LuaConfig LuaConfig::fromJson(const core::Json& section) {
    core::JsonValidator::requireKnownKeys(section, {"autoloads", "reload", "executionBudget", "memoryBudget"}, "the \"lua\" section of \"haylen.json\"");
    LuaConfig config;

    if (const auto autoloads = section.find("autoloads"); autoloads != section.end()) {
        const bool listsNames = autoloads->is_array() && std::ranges::all_of(*autoloads, &core::Json::is_string);
        if (!listsNames) {
            throw std::invalid_argument("The value of \"lua.autoloads\" in \"haylen.json\" must be a list of module names.");
        }

        config.autoloads = autoloads->get<std::vector<std::string>>();
        if (std::ranges::find(config.autoloads, std::string()) != config.autoloads.end()) {
            throw std::invalid_argument("The \"lua.autoloads\" list in \"haylen.json\" has an empty module name.");
        }
    }

    if (const auto reload = section.find("reload"); reload != section.end()) {
        const std::optional<Reload> mode = reload->is_string() ? reloadFromName(reload->get_ref<const std::string&>()) : std::nullopt;
        if (!mode) {
            throw std::invalid_argument("The \"lua.reload\" value " + reload->dump() + " in \"haylen.json\" is unknown. It is \"module\" or \"restart\".");
        }

        config.reload = *mode;
    }

    if (const auto seconds = section.find("executionBudget"); seconds != section.end()) {
        if (!isWithin(*seconds, kMinExecutionBudget, kMaxExecutionBudget)) {
            throw std::invalid_argument("The \"lua.executionBudget\" value in \"haylen.json\" must be a number of seconds from 0.1 to 600.");
        }

        config.executionBudget = std::chrono::milliseconds(std::llround(seconds->get<double>() * 1000.0));
    }

    if (const auto megabytes = section.find("memoryBudget"); megabytes != section.end()) {
        if (!isWithin(*megabytes, kMinMemoryBudget, kMaxMemoryBudget)) {
            throw std::invalid_argument("The \"lua.memoryBudget\" value in \"haylen.json\" must be a number of megabytes from 16 to 4096.");
        }

        config.memoryBudget = static_cast<std::size_t>(std::llround(megabytes->get<double>() * 1024.0 * 1024.0));
    }

    return config;
}

LuaConfig LuaConfig::fromConfig(const core::AppConfig& config) {
    const auto section = config.sections.find(kSection);
    return section != config.sections.end() ? fromJson(section->second) : LuaConfig{};
}

core::Json LuaConfig::toJson() const {
    return {{"autoloads", autoloads}, {"reload", reloadName(reload)}, {"executionBudget", static_cast<double>(executionBudget.count()) / 1000.0}, {"memoryBudget", static_cast<double>(memoryBudget) / (1024.0 * 1024.0)}};
}

std::optional<LuaConfig::Reload> LuaConfig::reloadFromName(std::string_view name) noexcept {
    const auto found = std::ranges::find(kReloadNames, name, &std::pair<std::string_view, Reload>::first);
    return found != kReloadNames.end() ? std::optional(found->second) : std::nullopt;
}

std::string_view LuaConfig::reloadName(Reload value) noexcept {
    return std::ranges::find(kReloadNames, value, &std::pair<std::string_view, Reload>::second)->first;
}

bool LuaConfig::isWithin(const core::Json& value, double minimum, double maximum) noexcept {
    return value.is_number() && value.get<double>() >= minimum && value.get<double>() <= maximum;
}

} // namespace haylen::lua
