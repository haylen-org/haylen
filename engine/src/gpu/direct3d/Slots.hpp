#pragma once

#include <cstddef>
#include <cstdint>
#include <format>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string_view>
#include <utility>
#include <vector>

namespace haylen::gpu::direct3d {

// The records of one kind of object of a device under the ids of their handles. The low bits of an id index its slot and the high bits count how often the slot was taken, so the id of a destroyed object never names the object that takes its slot next. Freed slots are taken again before new ones, so the storage stops growing once it holds the most objects that lived at once.
template <typename Record> class Slots final {
  public:
    explicit Slots(std::string_view kind) noexcept : name(kind) {}

    [[nodiscard]] std::uint32_t insert(Record record) {
        std::uint32_t index = 0;
        if (free.empty()) {
            if (slots.size() > kIndexMask) {
                throw std::runtime_error(std::format("The graphics device has no room for another {}.", name));
            }
            index = static_cast<std::uint32_t>(slots.size());
            slots.emplace_back();
        } else {
            index = free.back();
            free.pop_back();
        }

        Slot& slot = slots[index];
        slot.generation = slot.generation == kMaxGeneration ? 1 : slot.generation + 1;
        slot.record = std::move(record);
        ++count;
        return (slot.generation << kIndexBits) | index;
    }

    [[nodiscard]] Record* find(std::uint32_t id) noexcept {
        const std::uint32_t index = id & kIndexMask;
        if (index >= slots.size() || !slots[index].record || slots[index].generation != id >> kIndexBits) {
            return nullptr;
        }
        // A smart pointer of COM overloads its address operator, so the record takes its address apart.
        return std::addressof(*slots[index].record);
    }

    [[nodiscard]] Record& get(std::uint32_t id) {
        Record* record = find(id);
        if (record == nullptr) {
            throw std::logic_error(std::format("The {} {} does not exist.", name, id));
        }
        return *record;
    }

    // Takes the record out of its slot, or nothing when the id names no object.
    std::optional<Record> erase(std::uint32_t id) noexcept {
        if (find(id) == nullptr) {
            return std::nullopt;
        }
        const std::uint32_t index = id & kIndexMask;
        std::optional<Record> record = std::exchange(slots[index].record, std::nullopt);
        free.push_back(index);
        --count;
        return record;
    }

    template <typename Visit> void forEach(Visit&& visit) {
        for (Slot& slot : slots) {
            if (slot.record) {
                visit(*slot.record);
            }
        }
    }

    [[nodiscard]] std::size_t size() const noexcept {
        return count;
    }

    [[nodiscard]] std::size_t capacity() const noexcept {
        return slots.size();
    }

  private:
    static constexpr std::uint32_t kIndexBits = 20;
    static constexpr std::uint32_t kIndexMask = (1U << kIndexBits) - 1U;
    static constexpr std::uint32_t kMaxGeneration = (1U << (32U - kIndexBits)) - 1U;

    struct Slot {
        std::optional<Record> record;
        std::uint32_t generation = 0;
    };

    std::string_view name;
    std::vector<Slot> slots;
    std::vector<std::uint32_t> free;
    std::size_t count = 0;
};

} // namespace haylen::gpu::direct3d
