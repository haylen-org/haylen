#include "plugins/PlatformPlugin.hpp"

#include <algorithm>
#include <utility>

#include "haylen/core/Engine.hpp"
#include "platform/BridgeRelay.hpp"
#include "platform/DialogRelay.hpp"
#include "platform/DialogsLua.hpp"
#include "platform/PlatformLua.hpp"
#include "platform/SystemLua.hpp"

namespace haylen::plugins {

void PlatformPlugin::start(core::Engine& engine) {
    platform::BridgeRelay::attach(engine.getPlatform());
    platform::DialogRelay::attach(engine.getDialogs());
}

void PlatformPlugin::stop(core::Engine& engine) {
    platform::DialogRelay::detach(engine.getDialogs());
    platform::BridgeRelay::detach(engine.getPlatform());
    for (const std::shared_ptr<platform::VideoStream>& stream : videoStreams) {
        stream->detach();
    }
    videoStreams.clear();
}

void PlatformPlugin::installLua(core::Engine&, lua_State* L) {
    platform::PlatformLua::install(L);
    platform::SystemLua::install(L);
    platform::DialogsLua::install(L);
}

// The listeners of a frame may start watching more streams, which wait for the next frame.
void PlatformPlugin::beginFrame(core::Engine& engine, float) {
    const std::vector<std::shared_ptr<platform::VideoStream>> snapshot = videoStreams;
    for (const std::shared_ptr<platform::VideoStream>& stream : snapshot) {
        stream->update(engine.getGraphics());
    }
}

void PlatformPlugin::watch(std::shared_ptr<platform::VideoStream> stream) {
    if (std::ranges::find(videoStreams, stream) == videoStreams.end()) {
        videoStreams.push_back(std::move(stream));
    }
}

} // namespace haylen::plugins
