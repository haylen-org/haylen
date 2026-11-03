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
// clang-format on

namespace haylen::platform {

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

} // namespace haylen::platform

extern "C" {
EMSCRIPTEN_KEEPALIVE void haylen_web_displays_changed() {
    haylen::platform::WebEntry::guard("haylen_web_displays_changed", [] { haylen::platform::Player::postEvent({.type = haylen::platform::Event::Type::DisplaysChanged}); });
}
}
