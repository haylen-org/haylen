#include "haylen/plugins/VideoPlugin.hpp"

#include <algorithm>
#include <optional>
#include <stdexcept>
#include <utility>

#include "graphics/VideoLua.hpp"
#include "haylen/content/ContentManager.hpp"
#include "haylen/core/Engine.hpp"
#include "haylen/core/FrameClock.hpp"
#include "haylen/io/Package.hpp"
#include "platform/Host.hpp"

namespace haylen::plugins {

void VideoPlugin::stop(core::Engine&) {
    for (const std::shared_ptr<graphics::Video>& video : videos) {
        video->close();
    }
    videos.clear();
    updating.clear();
}

void VideoPlugin::installLua(core::Engine&, lua_State* L) {
    graphics::VideoLua::install(L);
}

// A video plays on real time, so the time scale never changes its speed, while its process mode decides whether the pause of the game holds it. Signals of a video may release videos, so the frame walks a copy.
void VideoPlugin::beginFrame(core::Engine& engine, float) {
    const core::FrameClock& clock = engine.getClock();
    updating.assign(videos.begin(), videos.end());
    for (const std::shared_ptr<graphics::Video>& video : updating) {
        const core::ProcessMode mode = video->getProcessMode() == core::ProcessMode::Inherit ? core::ProcessMode::Pausable : video->getProcessMode();
        video->update(clock.getUnscaledDelta(), clock.canProcess(mode));
    }
    updating.clear();
}

std::shared_ptr<graphics::Video> VideoPlugin::create(core::Engine& engine, const std::string& path, const graphics::Video::Options& options) {
    if (!engine.getPackage().assetExists(path)) {
        throw std::invalid_argument("The video \"" + path + "\" does not exist.");
    }
    auto video = std::make_shared<graphics::Video>(engine.getJobs(), engine.getGraphics(), engine.getAudio(), options);
    videos.push_back(video);

    // clang-format off
    graphics::Video::Opener opener = [&target = host, &package = engine.getPackage(), path] {
        return target.openVideo(std::shared_ptr<io::PackageReader>(package.openAssetReader(path)), path);
    };
    // clang-format on
    content::ContentManager& content = engine.getContent();
    if (content.isLocal(path)) {
        video->load(std::move(opener));
        return video;
    }

    // clang-format off
    content.acquire({path}, {}, [weak = std::weak_ptr<graphics::Video>(video), opener = std::move(opener)](std::optional<content::Error> error) mutable {
        const std::shared_ptr<graphics::Video> alive = weak.lock();
        if (!alive) {
            return;
        }
        if (error) {
            alive->load([message = std::string(error->what())]() -> std::unique_ptr<platform::VideoDecoder> { throw std::runtime_error(message); });
            return;
        }
        alive->load(std::move(opener));
    });
    // clang-format on
    return video;
}

void VideoPlugin::release(graphics::Video& video) {
    video.close();
    std::erase_if(videos, [&video](const std::shared_ptr<graphics::Video>& entry) { return entry.get() == &video; });
}

} // namespace haylen::plugins
