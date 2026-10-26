#pragma once

#include <cstdint>
#include <deque>
#include <format>
#include <optional>
#include <stdexcept>
#include <string_view>
#include <utility>
#include <vector>

namespace haylen::gpu::opengl {

// The records of one kind of object of the device under the ids of their handles. An id holds the slot of its record and the generation of the slot, so the handle of a destroyed object never reaches the record that took its slot until the generation wraps. Records never move while they live.
template <typename Record> class ObjectTable final {
  public:
    static constexpr std::uint32_t kIndexBits = 20;
    static constexpr std::uint32_t kIndexMask = (std::uint32_t{1} << kIndexBits) - 1;
    static constexpr std::uint32_t kGenerationMask = (std::uint32_t{1} << (32 - kIndexBits)) - 1;

    explicit ObjectTable(std::string_view kind) noexcept : name(kind) {}

    [[nodiscard]] std::uint32_t add(Record record) {
        if (freeSlots.empty()) {
            if (slots.size() >= kIndexMask) {
                throw std::runtime_error(std::format("The OpenGL device has no room for more {}.", name));
            }
            freeSlots.push_back(static_cast<std::uint32_t>(slots.size()));
            slots.emplace_back();
        }
        const std::uint32_t index = freeSlots.back();
        freeSlots.pop_back();
        Slot& slot = slots[index];
        slot.record.emplace(std::move(record));
        ++live;
        return (slot.generation << kIndexBits) | (index + 1);
    }

    [[nodiscard]] Record* find(std::uint32_t id) noexcept {
        const std::uint32_t index = (id & kIndexMask) - 1;
        if (index >= slots.size()) {
            return nullptr;
        }
        Slot& slot = slots[index];
        return slot.record && slot.generation == id >> kIndexBits ? &*slot.record : nullptr;
    }

    [[nodiscard]] Record& get(std::uint32_t id) {
        Record* record = find(id);
        if (record == nullptr) {
            throw std::logic_error(std::format("The OpenGL device has none of its {} under the id {}.", name, id));
        }
        return *record;
    }

    // Takes the record out of the table, or nothing when the id names no record.
    [[nodiscard]] std::optional<Record> remove(std::uint32_t id) {
        Record* record = find(id);
        if (record == nullptr) {
            return std::nullopt;
        }
        const std::uint32_t index = (id & kIndexMask) - 1;
        Slot& slot = slots[index];
        std::optional<Record> taken(std::move(*record));
        slot.record.reset();
        slot.generation = (slot.generation + 1) & kGenerationMask;
        freeSlots.push_back(index);
        --live;
        return taken;
    }

    template <typename Visitor> void forEach(Visitor&& visitor) {
        for (Slot& slot : slots) {
            if (slot.record) {
                visitor(*slot.record);
            }
        }
    }

    [[nodiscard]] std::string_view getName() const noexcept {
        return name;
    }

    [[nodiscard]] int size() const noexcept {
        return live;
    }

    [[nodiscard]] int capacity() const noexcept {
        return static_cast<int>(slots.size());
    }

  private:
    struct Slot {
        std::optional<Record> record;
        std::uint32_t generation = 0;
    };

    std::string_view name;
    std::deque<Slot> slots;
    std::vector<std::uint32_t> freeSlots;
    int live = 0;
};

} // namespace haylen::gpu::opengl
