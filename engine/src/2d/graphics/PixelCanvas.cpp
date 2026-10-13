#include "haylen/2d/graphics/PixelCanvas.hpp"

#include <stdexcept>

#include "haylen/graphics/Device.hpp"

namespace haylen::graphics2d {

PixelCanvas::PixelCanvas(graphics::Device& device, int width, int height, Options value) : size{static_cast<float>(width), static_cast<float>(height)}, options(value) {
    if (width < 1 || height < 1) {
        throw std::invalid_argument("A pixel canvas needs a width and a height of one pixel or more.");
    }
    target = device.createRenderTarget(width + 2, height + 2);
}

// The camera renders at the whole pixel nearest to where it is, and the part of a pixel it moved is what the draw shifts the image by. A camera anchored at the top-left corner steps back one pixel, so the border of the image lies around the view on that side too.
void PixelCanvas::begin(Renderer& renderer, const Camera& camera, const Renderer::CanvasOptions& canvasOptions) {
    Camera snapped = camera;
    snapped.viewport.reset();
    snapped.pixelSnap = true;
    const math::Vec2 zoom = camera.getZoom();
    const math::Vec2 exact = camera.getRenderPosition() * zoom;
    fraction = options.smooth ? exact - math::Vec2::round(exact) : math::Vec2{};
    if (snapped.anchor == Camera::Anchor::TopLeft) {
        snapped.position -= math::Vec2{1.0F, 1.0F} / zoom;
    }
    renderer.beginTarget(target, snapped, canvasOptions);
}

void PixelCanvas::draw(Renderer& renderer, const math::Rect& area, const DrawOrder& order) const {
    renderer.draw({.texture = target.getTexture(), .source = {1.0F + fraction.x, 1.0F + fraction.y, size.x, size.y}, .position = area.getMin(), .size = area.getSize(), .pivot = {0.0F, 0.0F}, .order = order});
}

} // namespace haylen::graphics2d
