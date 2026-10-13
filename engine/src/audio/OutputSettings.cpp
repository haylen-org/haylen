#include "haylen/audio/OutputSettings.hpp"

#include <algorithm>

namespace haylen::audio {

const std::array<std::pair<std::string_view, OutputSettings::Latency>, 3> OutputSettings::kLatencyNames{{{"low", Latency::Low}, {"balanced", Latency::Balanced}, {"powerSaving", Latency::PowerSaving}}};

std::optional<OutputSettings::Latency> OutputSettings::latencyFromName(std::string_view name) noexcept {
    const auto found = std::ranges::find(kLatencyNames, name, &std::pair<std::string_view, Latency>::first);
    return found != kLatencyNames.end() ? std::optional(found->second) : std::nullopt;
}

std::string_view OutputSettings::latencyName(Latency value) noexcept {
    return std::ranges::find(kLatencyNames, value, &std::pair<std::string_view, Latency>::second)->first;
}

} // namespace haylen::audio
