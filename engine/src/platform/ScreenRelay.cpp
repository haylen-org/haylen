#include "platform/ScreenRelay.hpp"

#include <utility>

#include "haylen/core/Log.hpp"

namespace haylen::platform {

std::mutex& ScreenRelay::mutex = *new std::mutex();
std::optional<ScreenRelay::Screen>& ScreenRelay::showing = *new std::optional<Screen>();
std::vector<ScreenRelay::Ending>& ScreenRelay::endings = *new std::vector<Ending>();

void ScreenRelay::show(Screen screen) {
    const std::scoped_lock lock(mutex);
    showing = std::move(screen);
}

std::optional<ScreenRelay::Screen> ScreenRelay::getShowing() {
    const std::scoped_lock lock(mutex);
    return showing;
}

void ScreenRelay::abandon(std::uint64_t id) {
    const std::scoped_lock lock(mutex);
    if (showing && showing->id == id) {
        showing->abandoned = true;
    }
}

void ScreenRelay::forget(std::uint64_t id) {
    const std::scoped_lock lock(mutex);
    if (showing && showing->id == id) {
        showing.reset();
    }
}

// The result is read on the thread of native code, like the answers of platform calls, and a second end of the same screen finds none showing.
void ScreenRelay::finish(std::uint64_t id, bool ok, std::string_view resultJson, std::vector<std::vector<std::byte>> buffers) {
    Bridge::Result result = Bridge::parseResult(ok, resultJson, std::move(buffers));
    const std::scoped_lock lock(mutex);
    if (!showing || showing->id != id) {
        return;
    }
    endings.push_back({.screen = std::move(*showing), .result = std::move(result)});
    showing.reset();
}

void ScreenRelay::restore(std::string plugin, std::string name, std::string_view stateJson, bool ok, std::string_view resultJson, std::vector<std::vector<std::byte>> buffers) {
    core::Json state = stateJson.empty() ? core::Json(nullptr) : core::Json::parse(stateJson, nullptr, false);
    if (plugin.empty() || name.empty() || state.is_discarded()) {
        core::Log::error("The platform restored a screen without its plugin, its name or valid JSON of its state, so its end was dropped.");
        return;
    }
    Bridge::Result result = Bridge::parseResult(ok, resultJson, std::move(buffers));
    const std::scoped_lock lock(mutex);
    endings.push_back({.screen = {.plugin = std::move(plugin), .name = std::move(name), .state = std::move(state)}, .result = std::move(result)});
}

std::vector<ScreenRelay::Ending> ScreenRelay::take() {
    const std::scoped_lock lock(mutex);
    return std::exchange(endings, {});
}

} // namespace haylen::platform
