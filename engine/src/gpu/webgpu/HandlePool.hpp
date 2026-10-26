#pragma once

#include <cstddef>
#include <cstdint>
#include <format>
#include <optional>
#include <stdexcept>
#include <string_view>
#include <utility>
#include <vector>

namespace haylen::gpu::webgpu {

// The records of one kind of object of the device behind the ids of their handles. An id holds the index of its slot and the generation of the slot, so the handle of a destroyed object never finds the object that takes its slot later, and id 0 names nothing.
template <typename Record> class HandlePool final {
  public:
    explicit HandlePool(std::string_view name) noexcept : kind(name) {}

    [[nodiscard]] std::uint32_t add(Record record) {
        std::uint32_t index = 0;
        if (free.empty()) {
            if (slots.size() >= kMaxSlots) {
                throw std::runtime_error(std::format("The graphics device has no room for another {}. At most {} can exist at once.", kind, kMaxSlots));
            }
            index = static_cast<std::uint32_t>(slots.size());
            slots.emplace_back();
        } else {
            index = free.back();
            free.pop_back();
        }

        Slot& slot = slots[index];
        slot.record = std::move(record);
        slot.live = true;
        ++live;
        return (slot.generation << kIndexBits) | (index + 1U);
    }

    [[nodiscard]] Record* find(std::uint32_t id) noexcept {
        const std::uint32_t index = (id & kIndexMask) - 1U;
        if (id == 0 || index >= slots.size()) {
            return nullptr;
        }
        Slot& slot = slots[index];
        return slot.live && slot.generation == id >> kIndexBits ? &slot.record : nullptr;
    }

    [[nodiscard]] const Record* find(std::uint32_t id) const noexcept {
        return const_cast<HandlePool*>(this)->find(id);
    }

    [[nodiscard]] Record& get(std::uint32_t id) {
        Record* record = find(id);
        if (record == nullptr) {
            throw std::logic_error(std::format("The {} {} does not exist.", kind, id));
        }
        return *record;
    }

    // Takes the record of the id out of the pool, or nothing when the id names no live object.
    [[nodiscard]] std::optional<Record> remove(std::uint32_t id) noexcept {
        Record* record = find(id);
        if (record == nullptr) {
            return std::nullopt;
        }
        const std::uint32_t index = (id & kIndexMask) - 1U;
        Slot& slot = slots[index];
        std::optional<Record> removed(std::move(*record));
        slot.record = Record{};
        slot.live = false;
        slot.generation = slot.generation == kMaxGeneration ? 1U : slot.generation + 1U;
        free.push_back(index);
        --live;
        return removed;
    }

    template <typename Visit> void forEach(Visit&& visit) {
        for (Slot& slot : slots) {
            if (slot.live) {
                visit(slot.record);
            }
        }
    }

    [[nodiscard]] std::string_view getKind() const noexcept {
        return kind;
    }

    [[nodiscard]] int size() const noexcept {
        return static_cast<int>(live);
    }

    [[nodiscard]] int capacity() const noexcept {
        return static_cast<int>(slots.capacity());
    }

  private:
    struct Slot {
        Record record{};
        std::uint32_t generation = 1;
        bool live = false;
    };

    static constexpr std::uint32_t kIndexBits = 20;
    static constexpr std::uint32_t kIndexMask = (1U << kIndexBits) - 1U;
    static constexpr std::size_t kMaxSlots = kIndexMask - 1U;
    static constexpr std::uint32_t kMaxGeneration = (1U << (32U - kIndexBits)) - 1U;

    std::string_view kind;
    std::vector<Slot> slots;
    std::vector<std::uint32_t> free;
    std::size_t live = 0;
};

} // namespace haylen::gpu::webgpu
