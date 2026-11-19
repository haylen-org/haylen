#include "gpu/DriverIssues.hpp"

#include <algorithm>
#include <charconv>
#include <system_error>

namespace haylen::gpu {

const std::array<DriverIssues::Entry, 9> DriverIssues::kEntries{{
    {
        .api = Device::Api::Vulkan,
        .issue = Issue::AvoidApi,
        .renderer = "PowerVR Rogue GE8320",
        .lastVersionNumber = 5776728,
        .reason = "The Vulkan driver of the PowerVR Rogue GE8320 fails some pipelines with \"VK_ERROR_OUT_OF_HOST_MEMORY\" and stops for good inside \"vkCreateGraphicsPipelines\" on others, which the driver 1.13@5776728 of Android 13 showed.",
    },
    {
        .api = Device::Api::Vulkan,
        .issue = Issue::UnboundedAcquire,
        .renderer = "Adreno (TM) 610",
        .lastVersionNumber = 0x8019F000,
        .reason = "The Vulkan swapchain of the Adreno 610 with the driver 512.415 takes no timeout in \"vkAcquireNextImageKHR\" but an endless one, which it waits for whatever the call asks, and logs \"non-infinite timeouts not yet implemented\" at every other timeout, which a phone of Android 11 showed in every frame.",
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
    {
        .api = Device::Api::OpenGlEs,
        .issue = Issue::NoGpuDisjoint,
        .renderer = "AMD Radeon",
        .version = "25.10.",
        .reason = "The OpenGL ES driver of AMD names \"GL_EXT_disjoint_timer_query\" and measures its queries, but refuses \"GL_GPU_DISJOINT_EXT\" in every getter with \"GL_INVALID_ENUM\", which the driver 25.10 of the AMD Radeon PRO V620 on Linux showed, so the GPU timer reads no disjoint state there, as on OpenGL.",
    },
    {
        .api = Device::Api::OpenGl,
        .issue = Issue::CompileOnCallingThread,
        .renderer = "Radeon",
        .version = "Core Profile Context",
        .reason = "The OpenGL driver of AMD hangs the GPU in the first frames that draw with new programs while its compiler threads still work on shaders it compiled before, so a GPU reset loses the context, which the drivers 25.10 on Linux and 25.1.1 on Windows of the AMD Radeon PRO V620 showed in every run with an empty shader cache, while the same frames finish with the compiling on the thread that asks for it.",
    },
    {
        .api = Device::Api::OpenGlEs,
        .issue = Issue::ClearLayersByDrawing,
        .renderer = "Android Emulator OpenGL ES Translator (Google SwiftShader)",
        .version = "SwiftShader 4.0.0",
        .reason = "The OpenGL ES translator of the Android emulator on SwiftShader 4.0.0.1 clears, reads and blits the first layer of an array or a volume texture whatever layer the framebuffer attaches, while its draws reach that layer, which the x86_64 images of Android 17 showed on a Linux host, so a pass clears an attachment at a later layer by drawing.",
    },
    {
        .api = Device::Api::OpenGl,
        .issue = Issue::NoAnisotropy,
        .renderer = "llvmpipe",
        .lastVersionNumber = makeMesaVersion(26, 0, 4095),
        .reason = "The llvmpipe of Mesa up to 26.0 takes the samples and the level of detail of anisotropic filtering from the slopes of the coordinate between neighboring pixels and ignores the slopes that \"textureGrad\" gives it, which Mesa 26.0.8 on Linux showed, where a lookup at points that jump from one pixel to the next read the smallest level.",
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

std::uint64_t DriverIssues::readMesaVersion(std::string_view version) noexcept {
    constexpr std::string_view kMarker = "Mesa ";
    const std::size_t start = version.find(kMarker);
    if (start == std::string_view::npos) {
        return 0;
    }

    // The version reads as numbers between dots, such as 26.0.8 in `Mesa 26.0.8-1ubuntu0.3`, and a part that is missing counts as 0.
    std::array<std::uint32_t, 3> parts{};
    const char* position = version.data() + start + kMarker.size();
    const char* const end = version.data() + version.size();
    for (std::size_t index = 0; index < parts.size(); ++index) {
        const std::from_chars_result read = std::from_chars(position, end, parts[index]);
        if (read.ec != std::errc() && index == 0) {
            return 0;
        }

        if (read.ec != std::errc()) {
            break;
        }

        position = read.ptr;
        if (position == end || *position != '.') {
            break;
        }

        ++position;
    }

    return makeMesaVersion(parts[0], parts[1], parts[2]);
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
