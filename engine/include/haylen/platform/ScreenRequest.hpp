#pragma once

#include <cstdint>
#include <string>

#include "haylen/core/Json.hpp"
#include "haylen/platform/Bridge.hpp"

namespace haylen::platform {

// The screen of a plugin that the app asks the platform to open, such as a paywall, a sign-in flow or a payment page, which shows over the app until it ends with one result. The platform keeps the id, the plugin, the screen and the state where they survive the end of the process, so a result that arrives after the app started again still reaches it with the state. An opaque screen hides the app completely.
struct ScreenRequest {
    std::uint64_t id = 0;
    std::string plugin;
    std::string screen;
    Bridge::Payload params;
    core::Json state;
    bool opaque = true;
};

} // namespace haylen::platform
