#pragma once

#include <array>
#include <memory>
#include <string_view>

#include "haylen/graphics/Device.hpp"
#include "haylen/graphics/Image.hpp"
#include "haylen/lua/Type.hpp"

struct lua_State;

namespace varn::async {
class Promise;
}

namespace haylen::lua {

template <> struct Type<graphics::Image> {
    static constexpr const char* name = "haylen.Image";
    using Storage = std::shared_ptr<graphics::Image>;
};

} // namespace haylen::lua

namespace haylen::graphics {

// Installs the `Image` class of `haylen.graphics`, pixels on the CPU. An image that work on the task pool still reads is copied before Lua changes it, so that work never sees a change, and Lua never waits for it.
class ImageLua final {
  public:
    static void install(lua_State* L);

    // Creates an image with `graphics.newImage(width, height, fill)`.
    static int create(lua_State* L);

    // Decodes the bytes of an image file with `graphics.decodeImage(bytes)`, and on the task pool with `graphics.decodeImageAsync(bytes)`, which returns a promise of the image.
    static int decode(lua_State* L);
    static int decodeAsync(lua_State* L);
    static int decodePlaceholder(lua_State* L);

    // Returns the image at the index as one Lua may change.
    [[nodiscard]] static Image& checkWritable(lua_State* L, int index);

    static void push(lua_State* L, Image image);

    // Reads a render target back with `graphics.readTarget(target)` and captures the next frame with `graphics.captureScreen()`, each returning a promise of an image.
    static int readTarget(lua_State* L);
    static int captureScreen(lua_State* L);

  private:
    static constexpr std::array<std::string_view, 2> kBlitFields{"source", "blend"};
    static constexpr std::array<std::string_view, 2> kPlaceholderFields{"columns", "rows"};
    // The colors a palette and the swatches take from an image without a count of their own.
    static constexpr int kPaletteColors = 8;
    static constexpr int kSwatchColors = 16;

    [[nodiscard]] static Image::Area readArea(lua_State* L, int index, const Image& image);

    // Returns the callback of a read that settles the promise with the image or the failure.
    [[nodiscard]] static Device::ReadCallback settle(const std::shared_ptr<varn::async::Promise>& promise);
    static int pixel(lua_State* L);
    static int setPixel(lua_State* L);
    static int fill(lua_State* L);
    static int blit(lua_State* L);
    static int blitAsync(lua_State* L);
    static int crop(lua_State* L);
    static int bleed(lua_State* L);
    static int visibleArea(lua_State* L);
    static int clone(lua_State* L);
    static int encodePng(lua_State* L);
    static int encodePngAsync(lua_State* L);
    static int palette(lua_State* L);
    static int swatches(lua_State* L);
    static int placeholder(lua_State* L);
    static int width(lua_State* L);
    static int height(lua_State* L);
};

} // namespace haylen::graphics
