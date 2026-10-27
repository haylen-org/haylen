#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <string_view>

#include "haylen/gpu/Device.hpp"

namespace haylen::gpu {

// The drivers whose bugs the devices work around, each with the reason that the tests on its GPU showed. An entry names the graphics API, a piece of the name the driver gives its GPU and, where the API numbers its drivers, the last version that has the bug, so a newer driver that fixed it leaves the entry.
class DriverIssues final {
  public:
    enum class Issue : std::uint8_t {
        // The device of the API refuses to start, so the platform draws through the next API of its order.
        AvoidApi,
        // The 32-bit float formats are not marked filterable, although the driver names an extension that says they are.
        NoFloatFiltering,
    };

    // A driver as its graphics API describes it: the name of its GPU and its version number, which is 0 where the API gives none.
    struct Driver {
        Device::Api api = Device::Api::None;
        std::string_view renderer;
        std::uint64_t versionNumber = 0;
    };

    // The reason of the entry of the issue that matches the driver, or nothing when its driver has no such issue.
    [[nodiscard]] static std::optional<std::string_view> find(const Driver& driver, Issue issue) noexcept;

  private:
    struct Entry {
        Device::Api api = Device::Api::None;
        Issue issue = Issue::AvoidApi;
        std::string_view renderer;
        std::uint64_t lastVersionNumber = 0;
        std::string_view reason;
    };

    static const std::array<Entry, 2> kEntries;

    [[nodiscard]] static bool matches(const Entry& entry, const Driver& driver, Issue issue) noexcept;
};

} // namespace haylen::gpu
