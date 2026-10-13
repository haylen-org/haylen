#pragma once

#include "haylen/2d/graphics/Camera.hpp"
#include "haylen/2d/graphics/DrawOrder.hpp"
#include "haylen/2d/graphics/Renderer.hpp"
#include "haylen/graphics/RenderTarget.hpp"
#include "haylen/math/Rect.hpp"
#include "haylen/math/Vec2.hpp"

namespace haylen::graphics {
class Device;
}

namespace haylen::graphics2d {

// A low-resolution canvas for pixel art that scrolls smoothly when it is scaled up. It renders the world through the camera at whole pixels into an image one pixel larger on every side, and its draw shifts the image by the part of a pixel the camera lies between, so the screen moves in its own small steps while every pixel of the art stays whole.
class PixelCanvas final {
  public:
    struct Options {
        // Shifts the image by the part of a pixel the camera lies between, which moves the view in steps of the screen instead of whole pixels of the canvas.
        bool smooth = true;
    };

    // Throws `std::invalid_argument` for a size below one pixel.
    PixelCanvas(graphics::Device& device, int width, int height, Options value);

    // Begins a render target canvas of the image through the camera at whole pixels, which ignores its viewport and rotates its view without the smooth shift.
    void begin(Renderer& renderer, const Camera& camera, const Renderer::CanvasOptions& canvasOptions);

    // Draws the image over the area of the open canvas, such as the screen, without its border.
    void draw(Renderer& renderer, const math::Rect& area, const DrawOrder& order = {}) const;

    [[nodiscard]] math::Vec2 getSize() const noexcept {
        return size;
    }
    [[nodiscard]] const graphics::RenderTarget& getTarget() const noexcept {
        return target;
    }

    // Returns the part of a pixel of the canvas the camera lay between at the last `begin`, which the draw shifts the image by.
    [[nodiscard]] math::Vec2 getFraction() const noexcept {
        return fraction;
    }
    [[nodiscard]] const Options& getOptions() const noexcept {
        return options;
    }

  private:
    graphics::RenderTarget target;
    math::Vec2 size{};
    Options options;
    math::Vec2 fraction{};
};

} // namespace haylen::graphics2d
