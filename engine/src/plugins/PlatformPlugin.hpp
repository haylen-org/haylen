#pragma once

#include <memory>
#include <vector>

#include "haylen/platform/VideoStream.hpp"
#include "haylen/plugins/Plugin.hpp"

namespace haylen::plugins {

// Connects the replies and events of native code and the answers of native dialogs to the app while it runs, keeps the textures of the video streams that the app draws current at the start of every frame, and installs `haylen.platform`, `haylen.system` and `haylen.dialogs`.
class PlatformPlugin final : public Plugin {
  public:
    [[nodiscard]] std::string_view getName() const noexcept override {
        return "platform";
    }
    void start(core::Engine& engine) override;
    void stop(core::Engine& engine) override;
    void installLua(core::Engine& engine, lua_State* L) override;
    void beginFrame(core::Engine& engine, float deltaSeconds) override;

    // Uploads the newest frame of the stream at the start of every frame from now on, until the app stops, which lets go of its texture and its listeners.
    void watch(std::shared_ptr<platform::VideoStream> stream);

  private:
    std::vector<std::shared_ptr<platform::VideoStream>> videoStreams;
};

} // namespace haylen::plugins
