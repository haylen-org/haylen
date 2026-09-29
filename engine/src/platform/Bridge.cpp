#include "haylen/platform/Bridge.hpp"

#include <format>
#include <stdexcept>
#include <utility>

#include "haylen/core/Log.hpp"

namespace haylen::platform {

std::atomic<std::uint64_t> Bridge::nextCallId{1};

bool Bridge::Mailbox::post(std::function<void()> task) const {
    const std::shared_ptr<Inbox> target = inbox.lock();
    if (!target) {
        return false;
    }
    const std::scoped_lock lock(target->mutex);
    target->tasks.push_back(std::move(task));
    return true;
}

Bridge::Bridge(Dispatcher native, Canceller cancel) : dispatcher(std::move(native)), canceller(std::move(cancel)) {
    if (!dispatcher || !canceller) {
        throw std::invalid_argument("The platform bridge needs a native dispatcher and a canceller.");
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

std::uint64_t Bridge::call(std::string_view method, const core::Json& params, Callback callback, std::optional<std::chrono::steady_clock::duration> timeout) {
    if (method.empty()) {
        throw std::invalid_argument("A platform call needs a method name.");
    }
    const std::uint64_t id = nextCallId.fetch_add(1);
    Pending entry{.callback = std::move(callback), .method = std::string(method), .native = !hasHandler(method)};
    if (timeout) {
        entry.deadline = std::chrono::steady_clock::now() + *timeout;
    }
    pending.emplace(id, std::move(entry));

    // clang-format off
    start(id, method, params, [shared = std::weak_ptr<Inbox>(inbox), id](Result result) {
        if (const std::shared_ptr<Inbox> alive = shared.lock()) {
            const std::scoped_lock lock(alive->mutex);
            alive->completions.push_back({id, std::move(result)});
        }
    });
    // clang-format on
    return id;
}

void Bridge::send(std::string_view method, const core::Json& params) {
    if (method.empty()) {
        throw std::invalid_argument("A platform call needs a method name.");
    }
    start(nextCallId.fetch_add(1), method, params, [](const Result&) {});
}

void Bridge::start(std::uint64_t id, std::string_view method, const core::Json& params, Reply reply) {
    const auto found = handlers.find(std::string(method));
    if (found == handlers.end()) {
        dispatcher(id, method, params.dump());
        return;
    }
    const Handler handler = found->second;
    handler(params, std::move(reply));
}

bool Bridge::cancel(std::uint64_t id) {
    const auto found = pending.find(id);
    if (found == pending.end()) {
        return false;
    }
    Pending entry = std::move(found->second);
    pending.erase(found);

    if (entry.native) {
        canceller(id, entry.method);
    }
    cancelled.emplace_back(std::move(entry.callback), Result{.error = {.message = std::format("The platform call {} was cancelled.", entry.method), .code = "cancelled"}});
    return true;
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

void Bridge::emit(std::string_view event, std::string_view payloadJson, bool retain) {
    core::Json payload = payloadJson.empty() ? core::Json(nullptr) : core::Json::parse(payloadJson, nullptr, false);
    if (payload.is_discarded()) {
        core::Log::error("The platform event '{}' carried invalid JSON and was dropped.", event);
        return;
    }

    const std::scoped_lock lock(inbox->mutex);
    inbox->events.push_back({std::string(event), std::move(payload), retain});
}

Bridge::Mailbox Bridge::getMailbox() const {
    return Mailbox(inbox);
}

void Bridge::pump() {
    std::vector<Completion> completions;
    std::vector<NativeEvent> events;
    std::vector<std::function<void()>> tasks;
    {
        const std::scoped_lock lock(inbox->mutex);
        completions.swap(inbox->completions);
        events.swap(inbox->events);
        tasks.swap(inbox->tasks);
    }

    // A reply for a call this bridge never made, or already settled, belongs to nobody and is dropped.
    for (Completion& completion : completions) {
        const auto found = pending.find(completion.id);
        if (found == pending.end()) {
            continue;
        }
        Callback callback = std::move(found->second.callback);
        pending.erase(found);
        if (callback) {
            callback(std::move(completion.result));
        }
    }

    // Retained events reach the listeners that connected since the last pump before any newer event of their name.
    std::vector<std::string> waiting;
    waiting.reserve(retained.size());
    for (const auto& [name, payloads] : retained) {
        waiting.push_back(name);
    }
    for (const std::string& name : waiting) {
        deliverRetained(name);
    }
    for (NativeEvent& event : events) {
        const auto found = signals.find(event.name);
        if (found != signals.end() && !found->second->empty()) {
            core::Signal<const core::Json&>& signal = *found->second;
            deliverRetained(event.name);
            signal.emit(event.payload);
            continue;
        }
        if (event.retain) {
            std::deque<core::Json>& payloads = retained[event.name];
            if (payloads.size() == kRetainedLimit) {
                payloads.pop_front();
            }
            payloads.push_back(std::move(event.payload));
        }
    }

    for (const std::function<void()>& task : tasks) {
        task();
    }

    for (auto& [callback, result] : std::exchange(cancelled, {})) {
        if (callback) {
            callback(std::move(result));
        }
    }
    expireCalls();
}

void Bridge::deliverRetained(const std::string& name) {
    const auto queued = retained.find(name);
    const auto found = signals.find(name);
    if (queued == retained.end() || found == signals.end() || found->second->empty()) {
        return;
    }

    // Listeners may connect to other events while they run, which moves the entries of the signal table but never the signals.
    core::Signal<const core::Json&>& signal = *found->second;
    const std::deque<core::Json> payloads = std::move(queued->second);
    retained.erase(queued);
    for (const core::Json& payload : payloads) {
        signal.emit(payload);
    }
}

std::size_t Bridge::getPendingCallCount() const {
    return pending.size() + cancelled.size();
}

void Bridge::expireCalls() {
    const auto now = std::chrono::steady_clock::now();
    std::vector<std::uint64_t> overdue;
    for (const auto& [id, entry] : pending) {
        if (entry.deadline && *entry.deadline <= now) {
            overdue.push_back(id);
        }
    }

    for (const std::uint64_t id : overdue) {
        const auto found = pending.find(id);
        if (found == pending.end()) {
            continue;
        }
        Pending entry = std::move(found->second);
        pending.erase(found);
        if (entry.native) {
            canceller(id, entry.method);
        }
        if (entry.callback) {
            entry.callback({.error = {.message = std::format("The platform call {} timed out.", entry.method), .code = "timeout"}});
        }
    }
}

// A failed call carries a plain message or an object with message, code and data, and any other payload still fails with a message.
Bridge::Error Bridge::readFailure(core::Json payload) {
    if (payload.is_string()) {
        return {.message = payload.get<std::string>()};
    }

    Error error{.message = "The native platform call failed without a message."};
    if (!payload.is_object()) {
        return error;
    }
    if (const auto message = payload.find("message"); message != payload.end() && message->is_string()) {
        error.message = message->get<std::string>();
    }
    if (const auto code = payload.find("code"); code != payload.end()) {
        error.code = std::move(*code);
    }
    if (const auto data = payload.find("data"); data != payload.end()) {
        error.data = std::move(*data);
    }
    return error;
}

Bridge::Result Bridge::parseResult(bool ok, std::string_view json) {
    core::Json parsed = json.empty() ? core::Json(nullptr) : core::Json::parse(json, nullptr, false);
    if (parsed.is_discarded()) {
        return {.error = {.message = "The platform returned invalid JSON.", .code = "invalidJson"}};
    }
    if (ok) {
        return {.ok = true, .value = std::move(parsed)};
    }
    return {.error = readFailure(std::move(parsed))};
}

} // namespace haylen::platform
