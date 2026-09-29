#include "platform/desktop/DesktopMethods.hpp"

#include <exception>

#include "platform/sokol/BridgeRelay.hpp"

namespace haylen::platform {

void DesktopMethods::dispatch(std::uint64_t call, std::string_view method, std::string_view paramsJson) {
    try {
        if (method == "device.info") {
            BridgeRelay::resolve(call, true, getDeviceInfo().dump());
            return;
        }
        if (method == "system.locale") {
            BridgeRelay::resolve(call, true, core::Json(getLocale()).dump());
            return;
        }
        if (method == "system.open_url") {
            const core::Json params = core::Json::parse(paramsJson, nullptr, false);
            const auto url = params.find("url");
            if (url == params.end() || !url->is_string() || url->get<std::string>().empty()) {
                fail(call, "The url is missing.");
                return;
            }
            // clang-format off
            openUrl(url->get<std::string>(), [call](bool opened) {
                if (!opened) {
                    fail(call, "The url could not be opened.");
                    return;
                }
                BridgeRelay::resolve(call, true, "true");
            });
            // clang-format on
            return;
        }
        if (method == "haptics.vibrate") {
            BridgeRelay::resolve(call, true, "null");
            return;
        }
    } catch (const std::exception& error) {
        fail(call, error.what());
        return;
    }
    fail(call, "No native handler is registered for " + std::string(method) + ".");
}

void DesktopMethods::fail(std::uint64_t call, const std::string& message) {
    BridgeRelay::resolve(call, false, core::Json{{"message", message}}.dump());
}

} // namespace haylen::platform
