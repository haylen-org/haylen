#pragma once

#include <concepts>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <string_view>
#include <typeindex>
#include <unordered_map>
#include <utility>
#include <vector>

#include "haylen/core/Connection.hpp"
#include "haylen/core/FrameQueue.hpp"
#include "haylen/core/Json.hpp"
#include "haylen/core/Signal.hpp"

namespace haylen::core {

// Publishes events to listeners that subscribe by name or, from C++, by type. Listeners run by descending priority, can listen to one channel or to all of them, can filter events and can consume an event so later listeners skip it. Events are delivered at once on the frame thread, or queued from any thread and delivered at the end of the frame. The engine publishes its lifecycle events here, with the names that LifecycleEvent lists.
class EventBus final {
  public:
    class Event final {
      public:
        [[nodiscard]] std::string_view getName() const noexcept {
            return name;
        }
        [[nodiscard]] std::string_view getChannel() const noexcept {
            return channel;
        }
        [[nodiscard]] const Json& getData() const noexcept {
            return data;
        }

        // Returns the native value the event carries when it has that type, such as the value of a typed event or the scene of a scene event, or null otherwise.
        template <typename T> [[nodiscard]] const T* get() const noexcept {
            return type == std::type_index(typeid(T)) ? static_cast<const T*>(value) : nullptr;
        }

        // Returns the type of the native value, which is void for events that carry only data.
        [[nodiscard]] std::type_index getType() const noexcept {
            return type;
        }

        void consume() noexcept {
            consumed = true;
        }
        [[nodiscard]] bool isConsumed() const noexcept {
            return consumed;
        }

      private:
        friend class EventBus;

        Event(std::string_view eventName, std::string_view eventChannel, const Json& eventData, const void* eventValue, std::type_index eventType) noexcept : name(eventName), channel(eventChannel), data(eventData), value(eventValue), type(eventType) {}

        std::string_view name;
        std::string_view channel;
        const Json& data;
        const void* value;
        std::type_index type;
        bool consumed = false;
    };

    using Handler = std::function<void(Event&)>;

    struct Options {
        // Listens only to events published on this channel. An empty channel listens to every channel.
        std::string channel;
        int priority = 0;
        bool once = false;

        // Unsubscribes once the owner is destroyed.
        std::weak_ptr<const void> owner;

        // Skips the events for which the filter returns false. A once listener stays until an event passes.
        std::function<bool(const Event&)> filter;
    };

    struct Topic {
        std::string name;
        std::size_t listeners = 0;
        std::uint64_t emissions = 0;
        std::size_t stale = 0;
    };

    explicit EventBus(FrameQueue& frameQueue);
    ~EventBus();

    EventBus(const EventBus&) = delete;
    EventBus& operator=(const EventBus&) = delete;

    Connection on(std::string_view name, Handler handler);
    Connection on(std::string_view name, Handler handler, Options options);

    template <typename T> Connection on(Handler handler) {
        return on<T>(std::move(handler), Options{});
    }
    template <typename T> Connection on(Handler handler, Options options) {
        return subscribe(state->typed[std::type_index(typeid(T))], std::move(handler), std::move(options));
    }

    // Delivers the event right away and returns whether a listener consumed it.
    bool emit(std::string_view name, const Json& data = {}, std::string_view channel = {});

    // Delivers a named event that also carries a native value, which listeners read with Event::get.
    template <typename T> bool emitWith(std::string_view name, const T& value, const Json& data = {}, std::string_view channel = {}) {
        return state->deliver(state->findNamed(name), Event(name, channel, data, &value, std::type_index(typeid(T))));
    }

    // Delivers a typed event to the listeners of its type.
    template <typename T>
        requires(!std::convertible_to<T, std::string_view>)
    bool emit(const T& value, std::string_view channel = {}) {
        return state->deliver(state->findTyped(std::type_index(typeid(T))), Event({}, channel, kNull, &value, std::type_index(typeid(T))));
    }

    // Queues the event from any thread. It is delivered on the frame thread at the end of the frame.
    void post(std::string name, Json data = {}, std::string channel = {});

    // Queues a named event that carries a native value, which is copied until the event is delivered.
    template <typename T> void postWith(std::string name, T value, std::string channel = {}) {
        // clang-format off
        queue.post([weak = std::weak_ptr<State>(state), name = std::move(name), value = std::move(value), channel = std::move(channel)] {
            if (const std::shared_ptr<State> target = weak.lock()) {
                target->deliver(target->findNamed(name), Event(name, channel, kNull, &value, std::type_index(typeid(T))));
            }
        });
        // clang-format on
    }

    template <typename T>
        requires(!std::convertible_to<T, std::string_view>)
    void post(T value, std::string channel = {}) {
        // clang-format off
        queue.post([weak = std::weak_ptr<State>(state), value = std::move(value), channel = std::move(channel)] {
            if (const std::shared_ptr<State> target = weak.lock()) {
                target->deliver(target->findTyped(std::type_index(typeid(T))), Event({}, channel, kNull, &value, std::type_index(typeid(T))));
            }
        });
        // clang-format on
    }

    // Removes every listener.
    void clear() noexcept;

    // Lists every named event that has ever had a listener, with its listener count, its emits since then and the listeners whose owner is gone.
    [[nodiscard]] std::vector<Topic> getTopics() const;

  private:
    using Listeners = Signal<Event&>;

    struct State {
        std::map<std::string, std::unique_ptr<Listeners>, std::less<>> named;
        std::unordered_map<std::type_index, std::unique_ptr<Listeners>> typed;

        [[nodiscard]] Listeners* findNamed(std::string_view name) const;
        [[nodiscard]] Listeners* findTyped(std::type_index type) const;
        static bool deliver(Listeners* listeners, Event event);
    };

    static const Json kNull;

    static Connection subscribe(std::unique_ptr<Listeners>& listeners, Handler handler, Options options);

    FrameQueue& queue;
    std::shared_ptr<State> state;
};

} // namespace haylen::core
