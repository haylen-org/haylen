#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <stdexcept>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

#include "haylen/core/Connection.hpp"
#include "haylen/core/FrameQueue.hpp"

namespace haylen::core {

// Type-safe multicast callback in the model of Godot signals. Slots run by descending priority and then in connection order. Slots may connect and disconnect while the signal emits: a slot disconnected by an earlier one is skipped, and a slot connected during an emit is first called by the next one. Emitting allocates nothing.
template <typename... Args> class Signal final {
  public:
    using Slot = std::function<void(Args...)>;

    struct Options {
        int priority = 0;

        // Disconnects the slot right before its first call.
        bool once = false;

        // Disconnects the slot once the owner is destroyed, before it would be called again.
        std::weak_ptr<const void> owner;

        // Delivers every call through this queue at the end of the frame instead of during the emit, with copies of the arguments. The slot is skipped when it disconnects before the queue runs.
        FrameQueue* queue = nullptr;
    };

    Signal() : state(std::make_shared<State>()) {}
    ~Signal() {
        clear();
    }

    Signal(const Signal&) = delete;
    Signal& operator=(const Signal&) = delete;

    Connection connect(Slot slot) {
        return connect(std::move(slot), Options{});
    }

    Connection connect(Slot slot, Options options) {
        if (!slot) {
            throw std::invalid_argument("A signal slot needs a function.");
        }
        if (options.queue != nullptr) {
            slot = deferTo(*options.queue, std::move(slot));
        }

        auto entry = std::make_shared<Entry>();
        entry->slot = std::move(slot);
        entry->priority = options.priority;
        entry->once = options.once;
        entry->tracked = !options.owner.expired();
        entry->owner = std::move(options.owner);
        entry->signal = state;

        // Slots connected during an emit wait outside the list the emit walks.
        std::vector<std::shared_ptr<Entry>>& destination = state->emitting > 0 ? state->added : state->entries;
        insertByPriority(destination, entry);
        return Connection(std::weak_ptr<Connection::Link>(entry));
    }

    void emit(Args... args) {
        State& current = *state;
        ++current.emissions;
        if (current.blocked) {
            return;
        }

        // The list is walked by index up to its size when the emit started, and it only shrinks once the outermost emit ends.
        const EmitGuard guard(state);
        const std::size_t count = current.entries.size();
        for (std::size_t index = 0; index < count; ++index) {
            Entry* entry = current.entries[index].get();
            if (!entry->connected || entry->blocked) {
                continue;
            }
            if (entry->tracked && entry->owner.expired()) {
                entry->disconnect();
                continue;
            }
            if (entry->once) {
                entry->disconnect();
            }
            entry->slot(args...);
        }
    }

    // Blocks every slot until unblocked. A blocked signal still counts its emits.
    void setBlocked(bool value) noexcept {
        state->blocked = value;
    }
    [[nodiscard]] bool isBlocked() const noexcept {
        return state->blocked;
    }

    void clear() noexcept {
        for (auto* list : {&state->entries, &state->added}) {
            for (const std::shared_ptr<Entry>& entry : *list) {
                entry->connected = false;
            }
        }
        state->added.clear();
        if (state->emitting == 0) {
            state->entries.clear();
        }
    }

    [[nodiscard]] std::size_t size() const noexcept {
        const auto live = [](const std::shared_ptr<Entry>& entry) { return entry->connected; };
        return static_cast<std::size_t>(std::count_if(state->entries.begin(), state->entries.end(), live) + std::count_if(state->added.begin(), state->added.end(), live));
    }
    [[nodiscard]] bool empty() const noexcept {
        return size() == 0;
    }

    // Counts every emit since the signal was created, blocked ones included.
    [[nodiscard]] std::uint64_t getEmissionCount() const noexcept {
        return state->emissions;
    }

    // Counts connected slots whose owner is already gone, which the next emit removes.
    [[nodiscard]] std::size_t getStaleCount() const noexcept {
        const auto stale = [](const std::shared_ptr<Entry>& entry) { return entry->connected && entry->tracked && entry->owner.expired(); };
        return static_cast<std::size_t>(std::count_if(state->entries.begin(), state->entries.end(), stale));
    }

  private:
    struct State;

    struct Entry final : Connection::Link {
        Slot slot;
        std::weak_ptr<const void> owner;
        std::weak_ptr<State> signal;
        int priority = 0;
        bool once = false;
        bool tracked = false;
        bool blocked = false;
        bool connected = true;

        void disconnect() override {
            if (!connected) {
                return;
            }
            connected = false;
            if (const std::shared_ptr<State> owning = signal.lock()) {
                owning->compact();
            }
        }
        [[nodiscard]] bool isConnected() const noexcept override {
            return connected && !signal.expired();
        }
        void setBlocked(bool value) override {
            blocked = value;
        }
        [[nodiscard]] bool isBlocked() const noexcept override {
            return blocked;
        }
    };

    struct State {
        std::vector<std::shared_ptr<Entry>> entries;
        std::vector<std::shared_ptr<Entry>> added;
        std::uint64_t emissions = 0;
        int emitting = 0;
        bool blocked = false;

        // Removes disconnected slots and merges the slots connected during emits, unless an emit is still walking the list.
        void compact() {
            if (emitting > 0) {
                return;
            }
            std::erase_if(entries, [](const std::shared_ptr<Entry>& entry) { return !entry->connected; });
            for (std::shared_ptr<Entry>& entry : std::exchange(added, {})) {
                if (entry->connected) {
                    insertByPriority(entries, std::move(entry));
                }
            }
        }
    };

    class EmitGuard final {
      public:
        explicit EmitGuard(std::shared_ptr<State> value) : guarded(std::move(value)) {
            ++guarded->emitting;
        }
        ~EmitGuard() {
            --guarded->emitting;
            guarded->compact();
        }

        EmitGuard(const EmitGuard&) = delete;
        EmitGuard& operator=(const EmitGuard&) = delete;

      private:
        std::shared_ptr<State> guarded;
    };

    static constexpr bool kCopyableArguments = (std::is_copy_constructible_v<std::decay_t<Args>> && ...) && !((std::is_lvalue_reference_v<Args> && !std::is_const_v<std::remove_reference_t<Args>>) || ...);

    static void insertByPriority(std::vector<std::shared_ptr<Entry>>& list, std::shared_ptr<Entry> entry) {
        const auto position = std::find_if(list.begin(), list.end(), [&entry](const std::shared_ptr<Entry>& existing) { return existing->priority < entry->priority; });
        list.insert(position, std::move(entry));
    }

    static Slot deferTo(FrameQueue& queue, Slot slot) {
        if constexpr (kCopyableArguments) {
            // clang-format off
            return [&queue, shared = std::make_shared<Slot>(std::move(slot))](Args... args) {
                queue.post([weak = std::weak_ptr<Slot>(shared), values = std::make_tuple(std::decay_t<Args>(args)...)] {
                    if (const std::shared_ptr<Slot> target = weak.lock()) {
                        std::apply(*target, values);
                    }
                });
            };
            // clang-format on
        } else {
            throw std::logic_error("This signal passes arguments by mutable reference, so it cannot deliver them later.");
        }
    }

    std::shared_ptr<State> state;
};

} // namespace haylen::core
