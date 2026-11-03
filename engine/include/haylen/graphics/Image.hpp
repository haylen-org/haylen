#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <vector>

#include "haylen/graphics/Texture.hpp"
#include "haylen/math/Color.hpp"

namespace haylen::graphics {

// CPU-side RGBA8 pixels, usually decoded on a worker thread before becoming a texture.
class Image final {
  public:
    // A rectangle of whole pixels.
    struct Area {
        int x = 0;
        int y = 0;
        int width = 0;
        int height = 0;

        [[nodiscard]] constexpr bool operator==(const Area&) const noexcept = default;
    };

    // Whether a blit replaces the pixels it covers or blends its pixels over them by their alpha.
    enum class BlitMode : std::uint8_t {
        Replace,
        Blend,
    };

    // The longest side of an image in pixels, which keeps every image within a size that memory and the pixel arithmetic of 32 bits hold.
    static constexpr int kMaxSide = 16384;

    Image() = default;
    Image(int imageWidth, int imageHeight, math::Color fill = math::Color::transparent());
    Image(int imageWidth, int imageHeight, std::vector<std::uint8_t> data);

    // Decodes PNG, JPG, TGA, BMP and GIF data into the buffer the decoder fills, checking the size its header declares before it allocates a pixel, so data from anywhere, such as a downloaded picture, can name the longest side it accepts. Throws `std::runtime_error` when the data is not a supported image or declares an image with a side longer than the largest side, and `std::invalid_argument` for a largest side under 1 or above `kMaxSide`.
    [[nodiscard]] static Image decode(std::span<const std::uint8_t> encoded, int maxSide = kMaxSide);

    // Turns the pixels a render target holds, premultiplied by alpha and with the bottom row first where the GPU device stores them so, into an image of straight colors with the top row first.
    [[nodiscard]] static Image fromRendered(std::vector<std::uint8_t> data, int columns, int rows, bool bottomUp);

    // Encodes the image as a PNG file.
    [[nodiscard]] std::vector<std::uint8_t> encodePng() const;

    // Decodes an image and prepares it for a texture with the options, off the frame thread: a linear filter bleeds the colors of the image into its transparent pixels, and mipmaps are made at once.
    [[nodiscard]] static Image decodeFor(std::span<const std::uint8_t> encoded, Texture::Options options);

    [[nodiscard]] int getWidth() const noexcept {
        return width;
    }
    [[nodiscard]] int getHeight() const noexcept {
        return height;
    }
    [[nodiscard]] bool isEmpty() const noexcept {
        return pixels.empty();
    }
    [[nodiscard]] std::span<const std::uint8_t> getPixels() const noexcept {
        return {pixels.data(), pixels.size()};
    }

    // Returns the pixels to change in place, which drops the mipmaps made from them.
    [[nodiscard]] std::span<std::uint8_t> getWritablePixels() noexcept {
        mipmaps.clear();
        return {pixels.data(), pixels.size()};
    }

    [[nodiscard]] math::Color getPixel(int x, int y) const noexcept;
    void setPixel(int x, int y, math::Color color) noexcept;

    [[nodiscard]] Area getArea() const noexcept {
        return {0, 0, width, height};
    }

    // Fills the part of the area inside the image with the color.
    void fill(const Area& area, math::Color color) noexcept;

    // Copies the area of the source to the position, clipped to both images, replacing the pixels it covers or blending over them. A source that is this image copies from a snapshot of it.
    void blit(const Image& source, const Area& from, int x, int y, BlitMode mode = BlitMode::Replace);

    // Returns a copy of an area. Throws `std::out_of_range` for an area that is not inside the image.
    [[nodiscard]] Image crop(const Area& area) const;

    // Repeats the edge pixels of the area outward by the amount on every side, inside the image, so filtered sampling at the edges of the area reads its own colors.
    void extrude(const Area& area, int amount) noexcept;

    // Gives the fully transparent pixels up to `kBleedReach` pixels around what shows the color of the nearest pixel that shows, keeping their alpha at zero, so linear filtering never pulls a dark or bright fringe from colors nobody sees. Mipmaps average by alpha, so pixels further away never reach them.
    void bleed();

    static constexpr int kBleedReach = 4;

    // Returns the smallest area that holds every pixel that shows, empty for an image where nothing shows.
    [[nodiscard]] Area getVisibleArea() const noexcept;

    // Returns the image at half its size in each direction, at least one pixel, averaging each block of two by two by the alpha of its pixels.
    [[nodiscard]] Image halved() const;

    // Makes the mipmaps of the image down to one pixel, which a texture created from it then takes instead of making them on the frame thread.
    void buildMipmaps();
    [[nodiscard]] std::span<const Image> getMipmaps() const noexcept {
        return mipmaps;
    }

  private:
    // The RGBA8 bytes of an image: a vector it was given or made, or the buffer the decoder filled, which a decoded image keeps instead of copying it.
    class Pixels final {
      public:
        Pixels() = default;
        explicit Pixels(std::vector<std::uint8_t> bytes) noexcept;
        Pixels(std::uint8_t* bytes, std::size_t size) noexcept;
        Pixels(const Pixels& other);
        Pixels(Pixels&& other) noexcept = default;
        Pixels& operator=(const Pixels& other);
        Pixels& operator=(Pixels&& other) noexcept = default;
        ~Pixels() = default;

        [[nodiscard]] std::uint8_t* data() noexcept {
            return adopted ? adopted.get() : owned.data();
        }
        [[nodiscard]] const std::uint8_t* data() const noexcept {
            return adopted ? adopted.get() : owned.data();
        }
        [[nodiscard]] std::size_t size() const noexcept {
            return adopted ? adoptedSize : owned.size();
        }
        [[nodiscard]] bool empty() const noexcept {
            return size() == 0;
        }
        [[nodiscard]] std::uint8_t& operator[](std::size_t index) noexcept {
            return data()[index];
        }
        [[nodiscard]] const std::uint8_t& operator[](std::size_t index) const noexcept {
            return data()[index];
        }

      private:
        struct Release {
            void operator()(std::uint8_t* bytes) const noexcept;
        };

        std::vector<std::uint8_t> owned;
        std::unique_ptr<std::uint8_t, Release> adopted;
        std::size_t adoptedSize = 0;
    };

    // The bytes past what an image needs that its decode may allocate, for the tables of the decoder and the buffers of tiny images.
    static constexpr std::size_t kDecodeSlack = 65536;

    Image(int imageWidth, int imageHeight, Pixels data) noexcept;

    [[nodiscard]] static std::size_t byteCount(int columns, int rows);
    [[nodiscard]] static Area clip(const Area& area, int columns, int rows) noexcept;

    int width = 0;
    int height = 0;
    Pixels pixels;
    std::vector<Image> mipmaps;
};

} // namespace haylen::graphics
