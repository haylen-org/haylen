#pragma once

#include <memory>
#include <string>
#include <vector>

#include "haylen/graphics/Video.hpp"
#include "haylen/plugins/Plugin.hpp"

namespace haylen::platform {
class Host;
}

namespace haylen::plugins {

// Plays the videos of the app with the decoder of the platform, keeping each one current at the start of every frame until it is released or the app stops, and installs `haylen.video`.
class VideoPlugin final : public Plugin {
  public:
    explicit VideoPlugin(platform::Host& host) : host(host) {}

    [[nodiscard]] std::string_view getName() const noexcept override {
        return "video";
    }
    void stop(core::Engine& engine) override;
    void installLua(core::Engine& engine, lua_State* L) override;
    void beginFrame(core::Engine& engine, float deltaSeconds) override;

    // Creates a video of a file of the package, by its path relative to `content`, which opens at once when the file is on the device and once the content manager brought it otherwise. Throws `std::invalid_argument` for invalid options and for an asset that does not exist.
    [[nodiscard]] std::shared_ptr<graphics::Video> create(core::Engine& engine, const std::string& path, const graphics::Video::Options& options);

    // Closes the video and stops keeping it current.
    void release(graphics::Video& video);

    [[nodiscard]] std::size_t getVideoCount() const noexcept {
        return videos.size();
    }

  private:
    platform::Host& host;
    std::vector<std::shared_ptr<graphics::Video>> videos;

    // The videos the frame updates, kept between frames so they allocate nothing.
    std::vector<std::shared_ptr<graphics::Video>> updating;
};

} // namespace haylen::plugins
