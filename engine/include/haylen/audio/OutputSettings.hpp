#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <string_view>
#include <utility>

namespace haylen::audio {

// How the audio output of an app trades latency for power and whether a limiter guards the `master` bus, which `app.json` sets in its `audio` object. Low latency plays a tap sooner with short periods of the device and the low latency mode of AAudio on Android, balanced is the default of the platform, and power saving plays with long periods that wake the processor less often.
struct OutputSettings {
    enum class Latency : std::uint8_t {
        Low,
        Balanced,
        PowerSaving,
    };

    Latency latency = Latency::Balanced;
    bool limiter = true;

    [[nodiscard]] static std::optional<Latency> latencyFromName(std::string_view name) noexcept;
    [[nodiscard]] static std::string_view latencyName(Latency value) noexcept;

  private:
    static const std::array<std::pair<std::string_view, Latency>, 3> kLatencyNames;
};

} // namespace haylen::audio
