#include "haylen/port/BridgeReplies.hpp"

#include <utility>

#include "platform/BridgeRelay.hpp"
#include "platform/ScreenRelay.hpp"

namespace haylen::port {

void BridgeReplies::resolve(std::uint64_t id, bool ok, std::string_view resultJson, std::vector<std::vector<std::byte>> buffers) {
    platform::BridgeRelay::resolve(id, ok, resultJson, std::move(buffers));
}

void BridgeReplies::emit(std::string_view event, std::string_view payloadJson, std::vector<std::vector<std::byte>> buffers, const platform::Bridge::EmitOptions& options) {
    platform::BridgeRelay::emit(event, payloadJson, std::move(buffers), options);
}

void BridgeReplies::finishScreen(std::uint64_t id, bool ok, std::string_view resultJson, std::vector<std::vector<std::byte>> buffers) {
    platform::ScreenRelay::finish(id, ok, resultJson, std::move(buffers));
}

void BridgeReplies::restoreScreen(std::string plugin, std::string name, std::string_view stateJson, bool ok, std::string_view resultJson, std::vector<std::vector<std::byte>> buffers) {
    platform::ScreenRelay::restore(std::move(plugin), std::move(name), stateJson, ok, resultJson, std::move(buffers));
}

} // namespace haylen::port
