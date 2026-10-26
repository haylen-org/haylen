#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <stdexcept>
#include <utility>
#include <vector>

namespace haylen::gpu::metal {

// The objects of one kind that a device made, under ids whose low bits name a slot and whose high bits count the objects that slot held, so the id of a destroyed object never finds the object that took its slot. Id 0 names nothing.
template <typename T> class ObjectPool final {
  public:
    // Takes the object and returns its id.
    [[nodiscard]] std::uint32_t add(T value) {
        std::uint32_t index = 0;
        if (free.empty()) {
            if (slots.size() >= kMaxSlots) {
                throw std::runtime_error("The graphics device has no room for another object of this kind.");
            }
            index = static_cast<std::uint32_t>(slots.size());
            slots.emplace_back();
        } else {
            index = free.back();
            free.pop_back();
        }

        Slot& slot = slots[index];
        slot.value = std::move(value);
        slot.alive = true;
        ++alive;
        return (slot.generation << kIndexBits) | (index + 1U);
    }

    [[nodiscard]] T* get(std::uint32_t id) noexcept {
        Slot* slot = find(id);
        return slot == nullptr ? nullptr : &slot->value;
    }

    [[nodiscard]] const T* get(std::uint32_t id) const noexcept {
        return const_cast<ObjectPool*>(this)->get(id);
    }

    // Takes the object out of the pool, whose slot then waits for another object, or nothing when the id names none.
    [[nodiscard]] std::optional<T> remove(std::uint32_t id) {
        Slot* slot = find(id);
        if (slot == nullptr) {
            return std::nullopt;
        }

        std::optional<T> value(std::move(slot->value));
        slot->value = T{};
        slot->alive = false;
        slot->generation = (slot->generation + 1U) & kGenerationMask;
        free.push_back(static_cast<std::uint32_t>(slot - slots.data()));
        --alive;
        return value;
    }

    [[nodiscard]] int size() const noexcept {
        return alive;
    }

    [[nodiscard]] int getCapacity() const noexcept {
        return static_cast<int>(slots.size());
    }

  private:
    static constexpr std::uint32_t kIndexBits = 20;
    static constexpr std::uint32_t kGenerationMask = (1U << (32U - kIndexBits)) - 1U;
    static constexpr std::size_t kMaxSlots = (std::size_t{1} << kIndexBits) - 1U;

    struct Slot {
        T value{};
        std::uint32_t generation = 0;
        bool alive = false;
    };

    [[nodiscard]] Slot* find(std::uint32_t id) noexcept {
        const std::uint32_t index = (id & ((1U << kIndexBits) - 1U)) - 1U;
        if (index >= slots.size()) {
            return nullptr;
        }
        Slot& slot = slots[index];
        return slot.alive && slot.generation == (id >> kIndexBits) ? &slot : nullptr;
    }

    std::vector<Slot> slots;
    std::vector<std::uint32_t> free;
    int alive = 0;
};

} // namespace haylen::gpu::metal
