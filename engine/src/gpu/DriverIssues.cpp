#include "gpu/DriverIssues.hpp"

#include <algorithm>

namespace haylen::gpu {

const std::array<DriverIssues::Entry, 2> DriverIssues::kEntries{{
    {
        .api = Device::Api::Vulkan,
        .issue = Issue::AvoidApi,
        .renderer = "PowerVR Rogue GE8320",
        .lastVersionNumber = 5776728,
        .reason = "The Vulkan driver of the PowerVR Rogue GE8320 fails some pipelines with \"VK_ERROR_OUT_OF_HOST_MEMORY\" and stops for good inside \"vkCreateGraphicsPipelines\" on others, which the driver 1.13@5776728 of Android 13 showed.",
    },
    {
        .api = Device::Api::OpenGlEs,
        .issue = Issue::NoFloatFiltering,
        .renderer = "Xclipse",
        .reason = "The ANGLE of the Xclipse GPUs names \"OES_texture_float_linear\" but samples the 32-bit float formats with the nearest texel, which the ANGLE of the Xclipse 530 of Android 14 showed.",
    },
}};

std::optional<std::string_view> DriverIssues::find(const Driver& driver, Issue issue) noexcept {
    // clang-format off
    const auto entry = std::ranges::find_if(kEntries, [&](const Entry& candidate) {
        return matches(candidate, driver, issue);
    });
    // clang-format on
    if (entry == kEntries.end()) {
        return std::nullopt;
    }
    return entry->reason;
}

bool DriverIssues::matches(const Entry& entry, const Driver& driver, Issue issue) noexcept {
    if (entry.api != driver.api || entry.issue != issue || driver.renderer.find(entry.renderer) == std::string_view::npos) {
        return false;
    }
    return entry.lastVersionNumber == 0 || driver.versionNumber <= entry.lastVersionNumber;
}

} // namespace haylen::gpu
