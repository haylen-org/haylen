#include "platform/web/WebDisplays.hpp"

#include <emscripten/emscripten.h>

#include <cstdlib>
#include <utility>

#include "haylen/core/Json.hpp"
#include "haylen/platform/Event.hpp"
#include "platform/Player.hpp"
#include "platform/web/WebEntry.hpp"

// clang-format off
EM_JS(char*, haylen_js_displays, (), {
    return stringToNewUTF8(Module.haylen.displays());
});

EM_JS(void, haylen_js_request_displays, (double id), {
    Module.haylen.requestDisplays(id);
});
// clang-format on

namespace haylen::platform {

std::unordered_map<std::uint64_t, std::function<void(Window::DisplayAccess)>>& WebDisplays::requests = *new std::unordered_map<std::uint64_t, std::function<void(Window::DisplayAccess)>>();
std::uint64_t WebDisplays::nextRequest = 1;

std::vector<Display> WebDisplays::getDisplays() {
    char* text = haylen_js_displays();
    const core::Json described = core::Json::parse(text);
    std::free(text);

    std::vector<Display> displays;
    for (const core::Json& item : described.at("displays")) {
        const core::Json& bounds = item.at("bounds");
        const core::Json& work = item.at("workArea");
        displays.push_back({
            .id = item.at("id").get<std::string>(),
            .name = item.at("name").get<std::string>(),
            .bounds = {bounds[0].get<float>(), bounds[1].get<float>(), bounds[2].get<float>(), bounds[3].get<float>()},
            .workArea = {work[0].get<float>(), work[1].get<float>(), work[2].get<float>(), work[3].get<float>()},
            .scale = item.at("scale").get<float>(),
            .primary = item.at("primary").get<bool>(),
        });
    }
    return displays;
}

std::string WebDisplays::getWindowDisplayId() {
    char* text = haylen_js_displays();
    const core::Json described = core::Json::parse(text);
    std::free(text);
    return described.at("current").get<std::string>();
}

void WebDisplays::request(std::function<void(Window::DisplayAccess access)> callback) {
    const std::uint64_t id = nextRequest++;
    requests.emplace(id, std::move(callback));
    haylen_js_request_displays(static_cast<double>(id));
}

void WebDisplays::answer(double id, Window::DisplayAccess access) {
    const auto found = requests.find(static_cast<std::uint64_t>(id));
    if (found == requests.end()) {
        return;
    }
    const std::function<void(Window::DisplayAccess)> callback = std::move(found->second);
    requests.erase(found);
    callback(access);
}

} // namespace haylen::platform

extern "C" {
EMSCRIPTEN_KEEPALIVE void haylen_web_displays_answered(double id, int access) {
    haylen::platform::WebEntry::guard("haylen_web_displays_answered", [&] { haylen::platform::WebDisplays::answer(id, static_cast<haylen::platform::Window::DisplayAccess>(access)); });
}

EMSCRIPTEN_KEEPALIVE void haylen_web_displays_changed() {
    haylen::platform::WebEntry::guard("haylen_web_displays_changed", [] { haylen::platform::Player::postEvent({.type = haylen::platform::Event::Type::DisplaysChanged}); });
}
}
