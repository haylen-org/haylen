#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>

namespace haylen::gpu::metal {

// The Metal libraries that the Apple build compiled from the Metal sources of the shader packages of the engine, for the kind of device it builds for, which `haylen_precompile_metal_libraries` generates.
class PrecompiledLibraries final {
  public:
    static constexpr std::size_t kKeySize = 32;

    // A library and the SHA-256 of the source it was compiled from.
    struct Library {
        std::array<std::uint8_t, kKeySize> key;
        std::span<const std::uint8_t> code;
    };

    // The library compiled from exactly this source, or `nullptr` when the build compiled none from it, such as for a shader of the app.
    [[nodiscard]] static const Library* find(std::string_view source);

  private:
    // The libraries sorted by key.
    [[nodiscard]] static std::span<const Library> getLibraries();
};

} // namespace haylen::gpu::metal
