#pragma once

#include <array>
#include <cstdint>
#include <memory>
#include <optional>
#include <string_view>
#include <utility>

#include "haylen/gpu/Device.hpp"
#include "haylen/math/Vec2.hpp"

namespace haylen::graphics {

struct TextureResource;

// Shared handle to a GPU texture. The GPU resource is released on the frame thread after the last handle goes away.
class Texture final {
  public:
    enum class Filter : std::uint8_t {
        Nearest,
        Linear,
    };

    // How the texture reads coordinates outside it, along each axis. `Border` reads the border color, on a device whose capabilities have border colors.
    enum class Wrap : std::uint8_t {
        Clamp,
        Repeat,
        Mirror,
        Border,
    };

    enum class BorderColor : std::uint8_t {
        TransparentBlack,
        OpaqueBlack,
        OpaqueWhite,
    };

    struct Options {
        Filter filter = Filter::Nearest;
        Wrap wrapU = Wrap::Clamp;
        Wrap wrapV = Wrap::Clamp;
        Wrap wrapW = Wrap::Clamp;

        // Keeps smaller copies of the image down to one pixel, which a texture drawn smaller than its size samples, with linear blending between them when the filter is linear. Unset, a texture made from an image keeps them when its filter is linear, so art drawn far below its size stays smooth, while pixel art with the nearest filter keeps its hard pixels. Render targets and textures that change in place never have them.
        std::optional<bool> mipmaps{};

        // The most samples of anisotropic filtering, which a texture drawn much smaller along one side than the other takes along its longer side. Unset, a texture with the linear filter and mipmaps takes the most that the device has, and the others take one.
        std::optional<int> anisotropy{};

        // The bias that the sampler adds to the level of detail of each lookup, which a negative value sharpens, on a device whose capabilities take one, and the levels of detail that lookups stay within.
        float lodBias = 0.0F;
        float minLod = 0.0F;
        float maxLod = 1000.0F;

        BorderColor borderColor = BorderColor::TransparentBlack;

        // Tells whether a texture made from an image with these options keeps mipmaps.
        [[nodiscard]] constexpr bool hasMipmaps() const noexcept {
            return mipmaps.value_or(filter == Filter::Linear);
        }

        // Sets the wrap of every axis.
        constexpr void setWrap(Wrap value) noexcept {
            wrapU = value;
            wrapV = value;
            wrapW = value;
        }

        [[nodiscard]] constexpr bool operator==(const Options&) const noexcept = default;
    };

    Texture() = default;
    explicit Texture(std::shared_ptr<TextureResource> value) noexcept : resource(std::move(value)) {}

    // Resolves the filter names `nearest` and `linear` and the wrap names `clamp`, `repeat` and `mirror`.
    [[nodiscard]] static std::optional<Filter> filterFromName(std::string_view name) noexcept;
    [[nodiscard]] static std::string_view filterName(Filter value) noexcept;
    [[nodiscard]] static std::optional<Wrap> wrapFromName(std::string_view name) noexcept;
    [[nodiscard]] static std::string_view wrapName(Wrap value) noexcept;
    [[nodiscard]] static std::optional<BorderColor> borderColorFromName(std::string_view name) noexcept;
    [[nodiscard]] static std::string_view borderColorName(BorderColor value) noexcept;

    // The name of a type of texture from `gpu::Device::kTextureTypeNames`: `flat`, `array`, `volume`, `cube` or `cubeArray`.
    [[nodiscard]] static std::string_view typeName(gpu::Device::TextureType value) noexcept;

    [[nodiscard]] bool isValid() const noexcept {
        return resource != nullptr;
    }
    [[nodiscard]] int getWidth() const noexcept;
    [[nodiscard]] int getHeight() const noexcept;
    [[nodiscard]] math::Vec2 getSize() const noexcept;
    [[nodiscard]] std::uint32_t getId() const noexcept;
    [[nodiscard]] Options getOptions() const noexcept;

    // The pixel format of the texture on the GPU, such as `Rgba8` for the textures of images and a compressed format for a texture made from its blocks.
    [[nodiscard]] gpu::Device::PixelFormat getFormat() const noexcept;

    // The type of the texture on the GPU: flat, an array of layers, a volume of slices, a cube or an array of cubes.
    [[nodiscard]] gpu::Device::TextureType getType() const noexcept;

    // The layers of an array, the six faces of a cube and six faces for each cube of a cube array, and 1 for a flat texture and a volume.
    [[nodiscard]] int getLayers() const noexcept;

    // The slices of a volume, and 1 for the other types.
    [[nodiscard]] int getDepth() const noexcept;

    // The mip levels of the texture, 1 for a texture without mipmaps.
    [[nodiscard]] int getMipLevels() const noexcept;

    // Whether the texture is a view of a range of the mip levels and layers of another texture.
    [[nodiscard]] bool isView() const noexcept;

    // The path of a texture that the assets loaded from the package, or an empty text for a texture made at run time.
    [[nodiscard]] std::string_view getLabel() const noexcept;
    [[nodiscard]] const std::shared_ptr<TextureResource>& getResource() const noexcept {
        return resource;
    }
    [[nodiscard]] bool operator==(const Texture& other) const noexcept {
        return resource == other.resource;
    }

  private:
    static const std::array<std::pair<std::string_view, Filter>, 2> kFilterNames;
    static const std::array<std::pair<std::string_view, Wrap>, 4> kWrapNames;
    static const std::array<std::pair<std::string_view, BorderColor>, 3> kBorderColorNames;

    std::shared_ptr<TextureResource> resource;
};

} // namespace haylen::graphics
