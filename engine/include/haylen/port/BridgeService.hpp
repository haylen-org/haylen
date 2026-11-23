#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "haylen/core/Json.hpp"
#include "haylen/platform/ScreenRequest.hpp"
#include "varn/dataview/DataView.h"

namespace haylen::port {

// The native parts of plugins in the language of the platform, which answer the calls of apps and open the screens of plugins. A port registers it where its platform runs such parts, and the engine answers every call and screen of a platform without it with the code `noHandler`, after the native libraries and the C++ plugins of the app had their turn. Its methods run on the frame thread, and the parts answer through `port::BridgeReplies` from any thread.
class BridgeService {
  public:
    BridgeService() = default;
    BridgeService(const BridgeService&) = delete;
    BridgeService& operator=(const BridgeService&) = delete;
    virtual ~BridgeService() = default;

    // The ids of the plugins whose native part the platform loaded.
    [[nodiscard]] virtual std::vector<std::string> getNativePlugins() = 0;

    // Hands a call to the handlers of the platform with the byte buffers and the data views that its parameters refer to, which stay valid until it returns, and the handler answers it exactly once through `BridgeReplies::resolve`. A method without a handler answers with the code `noHandler`.
    virtual void dispatch(std::uint64_t id, std::string_view method, std::string_view paramsJson, std::span<const std::vector<std::byte>> buffers, std::span<const varn::dataview::DataView> views) = 0;

    // Tells the handler of a call that the app gave it up.
    virtual void cancel(std::uint64_t id) = 0;

    // Opens the screen of a plugin over the app and ends it exactly once through `BridgeReplies::finishScreen`, and dismisses a screen that the app gave up where the platform can.
    virtual void openScreen(const platform::ScreenRequest& request) = 0;
    virtual void cancelScreen(std::uint64_t id) = 0;

    // Tells the native parts of plugins about a failure that stopped the app, with the JSON report of `core::AppError::toJson`.
    virtual void reportError(const core::Json& report) = 0;
};

} // namespace haylen::port
