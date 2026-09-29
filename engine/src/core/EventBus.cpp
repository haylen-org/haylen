#include "haylen/core/EventBus.hpp"

#include <stdexcept>

namespace haylen::core {

const Json& EventBus::kNull = *new const Json();

EventBus::EventBus(FrameQueue& frameQueue) : queue(frameQueue), state(std::make_shared<State>()) {}

EventBus::~EventBus() {
    clear();
}

EventBus::Listeners* EventBus::State::findNamed(std::string_view name) const {
    const auto found = named.find(name);
    return found == named.end() ? nullptr : found->second.get();
}

EventBus::Listeners* EventBus::State::findTyped(std::type_index type) const {
    const auto found = typed.find(type);
    return found == typed.end() ? nullptr : found->second.get();
}

bool EventBus::State::deliver(Listeners* listeners, Event event) {
    if (listeners != nullptr) {
        listeners->emit(event);
    }
    return event.isConsumed();
}

Connection EventBus::subscribe(std::unique_ptr<Listeners>& listeners, Handler handler, Options options) {
    if (!handler) {
        throw std::invalid_argument("An event listener needs a function.");
    }
    if (!listeners) {
        listeners = std::make_unique<Listeners>();
    }

    // A once listener disconnects itself after the first event that reaches it, since events of other channels or rejected by the filter do not count. The slot holds its own connection, which only refers back to the slot weakly.
    auto connection = std::make_shared<Connection>();
    // clang-format off
    *connection = listeners->connect([handler = std::move(handler), channel = std::move(options.channel), filter = std::move(options.filter), once = options.once, connection](Event& event) {
        if (event.isConsumed() || (!channel.empty() && channel != event.getChannel()) || (filter && !filter(event))) {
            return;
        }
        if (once) {
            connection->disconnect();
        }
        handler(event);
    }, {.priority = options.priority, .owner = std::move(options.owner)});
    // clang-format on
    return *connection;
}

Connection EventBus::on(std::string_view name, Handler handler) {
    return on(name, std::move(handler), Options{});
}

Connection EventBus::on(std::string_view name, Handler handler, Options options) {
    auto found = state->named.find(name);
    if (found == state->named.end()) {
        found = state->named.emplace(std::string(name), nullptr).first;
    }
    return subscribe(found->second, std::move(handler), std::move(options));
}

bool EventBus::emit(std::string_view name, const Json& data, std::string_view channel) {
    return State::deliver(state->findNamed(name), Event(name, channel, data, nullptr, std::type_index(typeid(void))));
}

void EventBus::post(std::string name, Json data, std::string channel) {
    // clang-format off
    queue.post([weak = std::weak_ptr<State>(state), name = std::move(name), data = std::move(data), channel = std::move(channel)] {
        if (const std::shared_ptr<State> target = weak.lock()) {
            State::deliver(target->findNamed(name), Event(name, channel, data, nullptr, std::type_index(typeid(void))));
        }
    });
    // clang-format on
}

void EventBus::clear() noexcept {
    for (const auto& [name, listeners] : state->named) {
        if (listeners) {
            listeners->clear();
        }
    }
    for (const auto& [type, listeners] : state->typed) {
        if (listeners) {
            listeners->clear();
        }
    }
}

std::vector<EventBus::Topic> EventBus::getTopics() const {
    std::vector<Topic> topics;
    topics.reserve(state->named.size());
    for (const auto& [name, listeners] : state->named) {
        topics.push_back({.name = name, .listeners = listeners->size(), .emissions = listeners->getEmissionCount(), .stale = listeners->getStaleCount()});
    }
    return topics;
}

} // namespace haylen::core
