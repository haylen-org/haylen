#include "haylen/platform/VideoStream.hpp"

#include <algorithm>
#include <cstring>
#include <stdexcept>
#include <utility>

#include "haylen/graphics/Device.hpp"
#include "haylen/graphics/Image.hpp"

namespace haylen::platform {

const graphics::Texture::Options VideoStream::kTextureOptions{.filter = graphics::Texture::Filter::Linear};

VideoStream::VideoStream(Format value, int width, int height) : format(value) {
    if (width < 0 || height < 0) {
        throw std::invalid_argument("A video stream cannot open with a negative size.");
    }
    shown.width = width;
    shown.height = height;
}

// The copy runs on the thread of the producer and outside the lock, so the frame thread only ever waits for the swap of two frames.
void VideoStream::push(const std::byte* pixels, int width, int height, std::size_t stride, double timestamp) {
    if (width <= 0 || height <= 0) {
        throw std::invalid_argument("A video frame needs a positive width and height.");
    }
    const std::size_t row = static_cast<std::size_t>(width) * 4;
    if (stride < row) {
        throw std::invalid_argument("The rows of a video frame are shorter than its width.");
    }

    Frame frame;
    {
        const std::scoped_lock lock(mutex);
        frame = std::move(spare);
    }
    frame.pixels.resize(row * static_cast<std::size_t>(height));
    for (std::size_t line = 0; line < static_cast<std::size_t>(height); ++line) {
        const std::byte* source = pixels + line * stride;
        std::uint8_t* target = frame.pixels.data() + line * row;
        if (format == Format::Rgba8) {
            std::memcpy(target, source, row);
            continue;
        }
        for (std::size_t offset = 0; offset < row; offset += 4) {
            target[offset] = std::to_integer<std::uint8_t>(source[offset + 2]);
            target[offset + 1] = std::to_integer<std::uint8_t>(source[offset + 1]);
            target[offset + 2] = std::to_integer<std::uint8_t>(source[offset]);
            target[offset + 3] = std::to_integer<std::uint8_t>(source[offset + 3]);
        }
    }
    frame.width = width;
    frame.height = height;
    frame.timestamp = timestamp;

    const std::scoped_lock lock(mutex);
    spare = std::move(latest);
    latest = std::move(frame);
    fresh = true;
}

bool VideoStream::update(graphics::Device& device) {
    {
        const std::scoped_lock lock(mutex);
        if (!fresh) {
            return false;
        }
        std::swap(shown, latest);
        fresh = false;
    }
    ++frameCount;

    if (!texture.isValid()) {
        createTexture(device);
    } else if (texture.getWidth() != shown.width || texture.getHeight() != shown.height) {
        device.replaceTexture(texture, graphics::Image(shown.width, shown.height, shown.pixels));
    } else {
        device.updateTexture(texture, shown.pixels);
    }
    frameReceived.emit(shown.timestamp);
    return true;
}

const graphics::Texture& VideoStream::getTexture(graphics::Device& device) {
    if (!texture.isValid()) {
        createTexture(device);
    }
    return texture;
}

void VideoStream::createTexture(graphics::Device& device) {
    if (shown.pixels.empty()) {
        texture = device.createDynamicTexture(std::max(shown.width, 1), std::max(shown.height, 1), math::Color::transparent(), kTextureOptions);
        return;
    }
    texture = device.createDynamicTexture(graphics::Image(shown.width, shown.height, shown.pixels), kTextureOptions);
}

void VideoStream::detach() {
    texture = {};
    frameReceived.clear();
}

} // namespace haylen::platform
