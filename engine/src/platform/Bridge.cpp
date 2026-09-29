#include "haylen/platform/Bridge.hpp"

#include <stdexcept>
#include <utility>

#include "haylen/core/Log.hpp"

namespace haylen::platform {

std::atomic<std::uint64_t> Bridge::nextCallId{1};

Bridge::Bridge(Dispatcher native) : dispatcher(std::move(native)) {
    if (!dispatcher) {
        throw std::invalid_argument("The platform bridge needs a native dispatcher.");
    }
}

void Bridge::registerHandler(std::string method, Handler handler) {
    if (method.empty() || !handler) {
        throw std::invalid_argument("A platform handler needs a method name and a function.");
    }
    handlers.insert_or_assign(std::move(method), std::move(handler));
}

bool Bridge::hasHandler(std::string_view method) const {
    return handlers.contains(std::string(method));
}

std::uint64_t Bridge::call(std::string_view method, const core::Json& params, Callback callback) {
    if (method.empty()) {
        throw std::invalid_argument("A platform call needs a method name.");
    }
    const std::uint64_t id = nextCallId.fetch_add(1);
    if (callback) {
        callbacks.emplace(id, std::move(callback));
    }

    if (const auto found = handlers.find(std::string(method)); found != handlers.end()) {
        // clang-format off
        found->second(params, [shared = std::weak_ptr<Inbox>(inbox), id](Result result) {
            if (const std::shared_ptr<Inbox> alive = shared.lock()) {
                const std::scoped_lock lock(alive->mutex);
                alive->completions.push_back({id, std::move(result)});
            }
        });
        // clang-format on
        return id;
    }

    dispatcher(id, method, params.dump());
    return id;
}

core::Connection Bridge::on(const std::string& event, std::function<void(const core::Json&)> listener) {
    auto& signal = signals[event];
    if (!signal) {
        signal = std::make_unique<core::Signal<const core::Json&>>();
    }
    return signal->connect(std::move(listener));
}

void Bridge::resolve(std::uint64_t id, bool ok, std::string_view resultJson) {
    Result result = parseResult(ok, resultJson);
    const std::scoped_lock lock(inbox->mutex);
    inbox->completions.push_back({id, std::move(result)});
}

void Bridge::emit(std::string_view event, std::string_view payloadJson) {
    core::Json payload = payloadJson.empty() ? core::Json(nullptr) : core::Json::parse(payloadJson, nullptr, false);
    if (payload.is_discarded()) {
        core::Log::error("The platform event '{}' carried invalid JSON and was dropped.", event);
        return;
    }

    const std::scoped_lock lock(inbox->mutex);
    inbox->events.push_back({std::string(event), std::move(payload)});
}

void Bridge::pump() {
    std::vector<Completion> completions;
    std::vector<NativeEvent> events;
    {
        const std::scoped_lock lock(inbox->mutex);
        completions.swap(inbox->completions);
        events.swap(inbox->events);
    }

    // A reply for a call this bridge never made, or already answered, belongs to nobody and is dropped.
    for (Completion& completion : completions) {
        const auto found = callbacks.find(completion.id);
        if (found == callbacks.end()) {
            continue;
        }
        Callback callback = std::move(found->second);
        callbacks.erase(found);
        callback(std::move(completion.result));
    }

    for (const NativeEvent& event : events) {
        if (const auto found = signals.find(event.name); found != signals.end()) {
            found->second->emit(event.payload);
        }
    }
}

std::size_t Bridge::getPendingCallCount() const {
    return callbacks.size();
}

// A failed call carries a plain message or an object with a message field, and any other payload still fails with a message.
std::string Bridge::getFailureMessage(const core::Json& payload) {
    if (payload.is_string()) {
        return payload.get<std::string>();
    }
    if (payload.is_object()) {
        if (const auto message = payload.find("message"); message != payload.end() && message->is_string()) {
            return message->get<std::string>();
        }
    }
    return "The native platform call failed without a message.";
}

Bridge::Result Bridge::parseResult(bool ok, std::string_view json) {
    core::Json parsed = json.empty() ? core::Json(nullptr) : core::Json::parse(json, nullptr, false);
    if (parsed.is_discarded()) {
        return {.ok = false, .error = "The platform returned invalid JSON."};
    }
    if (ok) {
        return {.ok = true, .value = std::move(parsed)};
    }
    std::string message = getFailureMessage(parsed);
    return {.ok = false, .value = std::move(parsed), .error = std::move(message)};
}

} // namespace haylen::platform
