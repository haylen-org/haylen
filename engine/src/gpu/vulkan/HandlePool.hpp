#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <stdexcept>
#include <vector>

namespace haylen::gpu::vulkan {

// The records of the objects of one kind behind the ids of their handles. An id holds the slot of its record and the generation of that slot, so the id of a destroyed object never finds the record that takes its slot later, and no id is 0.
template <typename Record> class HandlePool final {
  public:
    [[nodiscard]] std::uint32_t add(std::unique_ptr<Record> record) {
        std::uint32_t index = 0;
        if (free.empty()) {
            if (slots.size() >= kMaxSlots) {
                throw std::runtime_error("The Vulkan device has no room for another object of this kind.");
            }
            index = static_cast<std::uint32_t>(slots.size());
            slots.emplace_back();
        } else {
            index = free.back();
            free.pop_back();
        }
        Slot& slot = slots[index];
        slot.record = std::move(record);
        ++live;
        return (slot.generation << kIndexBits) | (index + 1);
    }

    [[nodiscard]] Record* find(std::uint32_t id) const noexcept {
        const std::uint32_t index = (id & kIndexMask) - 1;
        if (id == 0 || index >= slots.size() || slots[index].generation != id >> kIndexBits) {
            return nullptr;
        }
        return slots[index].record.get();
    }

    // Takes the record out of its slot, or gives null when the id names no record.
    [[nodiscard]] std::unique_ptr<Record> remove(std::uint32_t id) {
        if (find(id) == nullptr) {
            return nullptr;
        }
        const std::uint32_t index = (id & kIndexMask) - 1;
        Slot& slot = slots[index];
        slot.generation = (slot.generation + 1) & kGenerationMask;
        free.push_back(index);
        --live;
        return std::move(slot.record);
    }

    // Calls the function with every record that exists.
    template <typename Function> void forEach(Function&& function) const {
        for (const Slot& slot : slots) {
            if (slot.record) {
                function(*slot.record);
            }
        }
    }

    [[nodiscard]] int size() const noexcept {
        return live;
    }

    [[nodiscard]] int capacity() const noexcept {
        return static_cast<int>(slots.size());
    }

  private:
    static constexpr std::uint32_t kIndexBits = 20;
    static constexpr std::uint32_t kIndexMask = (1U << kIndexBits) - 1;
    static constexpr std::uint32_t kGenerationMask = (1U << (32 - kIndexBits)) - 1;
    static constexpr std::size_t kMaxSlots = kIndexMask;

    struct Slot {
        std::unique_ptr<Record> record;
        std::uint32_t generation = 0;
    };

    std::vector<Slot> slots;
    std::vector<std::uint32_t> free;
    int live = 0;
};

} // namespace haylen::gpu::vulkan
