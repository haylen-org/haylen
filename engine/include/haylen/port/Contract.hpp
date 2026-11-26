#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <string_view>

namespace haylen::port {

// The contract between the engine and its ports: the headers of `haylen/port`, the function `haylen_port` of CMake and what a platform declares there. A port names the version it follows in the `CONTRACT` of its `haylen_port`, which the configuration of the engine compares with `kVersion`, and the version grows whenever the contract changes in a way that a port must follow.
class Contract final {
  public:
    static constexpr int kVersion = 17;

    // What a platform allows apps to do beyond what every platform allows, which a port declares in the `CAPABILITIES` of its `haylen_port`: loading libraries at run time, writing code into memory and starting other processes.
    enum class Capability : std::uint8_t {
        DynamicLibraries,
        ExecutableMemory,
        Processes,
    };

    static constexpr std::array<std::string_view, 3> kCapabilityNames{"dynamicLibraries", "executableMemory", "processes"};

    [[nodiscard]] static std::string_view getCapabilityName(Capability value) noexcept;
    [[nodiscard]] static std::optional<Capability> findCapability(std::string_view name) noexcept;

    // Whether the platform of this build allows the capability, as its port declares it or as the engine knows it of its own platforms.
    [[nodiscard]] static bool allows(Capability value) noexcept;

    // Whether the platform of this build runs threads besides the one that plays the frames, which a page does not and a port declares in `THREADS`.
    [[nodiscard]] static bool hasThreads() noexcept;
};

} // namespace haylen::port
