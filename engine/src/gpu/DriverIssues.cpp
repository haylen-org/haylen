#include "gpu/DriverIssues.hpp"

#include <algorithm>

namespace haylen::gpu {

const std::array<DriverIssues::Entry, 4> DriverIssues::kEntries{{
    {
        .api = Device::Api::Vulkan,
        .issue = Issue::AvoidApi,
        .renderer = "PowerVR Rogue GE8320",
        .lastVersionNumber = 5776728,
        .reason = "The Vulkan driver of the PowerVR Rogue GE8320 fails some pipelines with \"VK_ERROR_OUT_OF_HOST_MEMORY\" and stops for good inside \"vkCreateGraphicsPipelines\" on others, which the driver 1.13@5776728 of Android 13 showed.",
    },
    {
        .api = Device::Api::Vulkan,
        .issue = Issue::StoreKeptDepth,
        .renderer = "Adreno (TM) 630",
        .lastVersionNumber = 0x8014B000,
        .reason = "The Vulkan driver of the Adreno 630 reads the depth that a pass keeps as cleared when the pass discards that depth and clears the stencil of a format without stencil, which the driver 512.331 of Android 9 showed, while a pass that also stores the depth and keeps the stencil reads the depth it kept.",
    },
    {
        .api = Device::Api::OpenGlEs,
        .issue = Issue::NoFloatFiltering,
        .renderer = "Xclipse",
        .reason = "The ANGLE of the Xclipse GPUs names \"OES_texture_float_linear\" but samples the 32-bit float formats with the nearest texel, which the ANGLE of the Xclipse 530 of Android 14 showed.",
    },
    {
        .api = Device::Api::OpenGlEs,
        .issue = Issue::FlushEachPass,
        .renderer = "Mali-G72",
        .version = "v1.r19p0",
        .reason = "The OpenGL ES driver r19p0 of the Mali-G72 resets the context as guilty in some draws of a pass that follows another pass whose commands it still holds, and loses the frame, which the Mali-G72 of Android 10 showed, while a flush at the end of each pass keeps every pass.",
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
    if (!entry.version.empty() && driver.version.find(entry.version) == std::string_view::npos) {
        return false;
    }
    return entry.lastVersionNumber == 0 || driver.versionNumber <= entry.lastVersionNumber;
}

} // namespace haylen::gpu
