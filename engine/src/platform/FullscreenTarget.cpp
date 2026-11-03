#include "platform/FullscreenTarget.hpp"

#include <algorithm>
#include <format>
#include <stdexcept>
#include <string>

namespace haylen::platform {

FullscreenTarget FullscreenTarget::resolve(std::span<const Display> displays, std::string_view windowDisplay, const Window::Fullscreen& request) {
    const std::string id(request.display.empty() ? windowDisplay : std::string_view(request.display));
    const auto found = std::ranges::find(displays, id, &Display::id);
    if (found == displays.end()) {
        throw std::invalid_argument("There is no display \"" + id + "\" to go fullscreen on. The function \"display.list()\" lists the displays.");
    }

    FullscreenTarget target{.display = *found};
    if (request.mode) {
        const Display::Mode& wanted = *request.mode;
        const Display::Mode* mode = found->findMode(wanted.width, wanted.height, wanted.refreshRate);
        if (mode == nullptr) {
            const std::string rate = wanted.refreshRate > 0.0F ? std::format(" at {} frames per second", wanted.refreshRate) : std::string();
            throw std::invalid_argument(std::format("The display \"{}\" has no mode of {} by {} pixels{}. The modes of \"display.list()\" are the ones it runs in.", id, wanted.width, wanted.height, rate));
        }
        target.mode = *mode;
    }
    return target;
}

} // namespace haylen::platform
