#include "platform/BridgeRelay.hpp"

#include <utility>

#include "haylen/core/Json.hpp"
#include "varn/dataview/DataViewJson.h"

namespace haylen::platform {

std::mutex& BridgeRelay::mutex = *new std::mutex();
Bridge* BridgeRelay::bridge = nullptr;
KeptEvents& BridgeRelay::kept = *new KeptEvents();

void BridgeRelay::attach(Bridge& value) {
    const std::scoped_lock lock(mutex);
    bridge = &value;

    for (KeptEvents::Event& event : kept.take()) {
        bridge->emit(event.name, event.json, std::move(event.buffers), event.options);
    }
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

// A kept event holds its own references to the data views that its JSON names, since the library may let them go before the next app starts.
void BridgeRelay::emitOrKeep(KeptEvents::Event event) {
    const std::scoped_lock lock(mutex);
    if (bridge != nullptr) {
        bridge->emit(event.name, event.json, std::move(event.buffers), event.options);
        return;
    }

    const core::Json payload = core::Json::parse(event.json, nullptr, false);
    if (!payload.is_discarded()) {
        event.views = varn::dataview::DataViewJson::acquire(payload);
    }

    kept.keep(std::move(event));
}

} // namespace haylen::platform
