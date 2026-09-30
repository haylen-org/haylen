#include "platform/BridgeRelay.hpp"

#include <utility>

namespace haylen::platform {

std::mutex& BridgeRelay::mutex = *new std::mutex();
Bridge* BridgeRelay::bridge = nullptr;

void BridgeRelay::attach(Bridge& value) noexcept {
    const std::scoped_lock lock(mutex);
    bridge = &value;
}

void BridgeRelay::detach(const Bridge& value) noexcept {
    const std::scoped_lock lock(mutex);
    if (bridge == &value) {
        bridge = nullptr;
    }
}

void BridgeRelay::resolve(std::uint64_t id, bool ok, std::string_view resultJson, std::vector<std::vector<std::byte>> buffers) {
    const std::scoped_lock lock(mutex);
    if (bridge != nullptr) {
        bridge->resolve(id, ok, resultJson, std::move(buffers));
    }
}

void BridgeRelay::emit(std::string_view event, std::string_view payloadJson, std::vector<std::vector<std::byte>> buffers, const Bridge::EmitOptions& options) {
    const std::scoped_lock lock(mutex);
    if (bridge != nullptr) {
        bridge->emit(event, payloadJson, std::move(buffers), options);
    }
}

} // namespace haylen::platform
