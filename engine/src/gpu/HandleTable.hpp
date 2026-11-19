#pragma once

#include <cstddef>
#include <cstdint>
#include <deque>
#include <format>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string_view>
#include <utility>
#include <vector>

namespace haylen::gpu {

// The records of one kind of object of a device under the ids of their handles. The low bits of an id name the slot of its record and the high bits the generation of that slot, which grows each time the slot is freed, so the id of a destroyed object never finds the record that takes its slot later. The generation wraps only after a slot was taken 2^40 times, which no app reaches. Id 0 names nothing, and records never move while they live.
template <typename Record> class HandleTable final {
  public:
    static constexpr std::uint32_t kIndexBits = 24;
    static constexpr std::uint64_t kIndexMask = (std::uint64_t{1} << kIndexBits) - 1;
    static constexpr std::uint64_t kGenerationMask = (std::uint64_t{1} << (64 - kIndexBits)) - 1;
    static constexpr std::size_t kMaxSlots = kIndexMask;

    explicit HandleTable(std::string_view kind) noexcept : name(kind) {}
    HandleTable(const HandleTable&) = delete;
    HandleTable& operator=(const HandleTable&) = delete;

    // Takes the record only once it has a slot, so a caller whose record finds no room still holds it and releases what it owns.
    [[nodiscard]] std::uint64_t add(Record&& record) {
        if (freeSlots.empty()) {
            if (slots.size() >= kMaxSlots) {
                throw std::runtime_error(std::format("The graphics device has no room for another {}. At most {} can exist at once.", name, kMaxSlots));
            }

            freeSlots.push_back(slots.size());
            slots.emplace_back();
        }

        const std::size_t index = freeSlots.back();
        freeSlots.pop_back();

        Slot& slot = slots[index];
        slot.record.emplace(std::move(record));
        ++live;
        return (slot.generation << kIndexBits) | (index + 1);
    }

    [[nodiscard]] Record* find(std::uint64_t id) noexcept {
        Slot* slot = findSlot(id);
        if (slot == nullptr) {
            return nullptr;
        }

        // A smart pointer of COM overloads its address operator, so the record takes its address apart.
        return std::addressof(*slot->record);
    }

    [[nodiscard]] const Record* find(std::uint64_t id) const noexcept {
        return const_cast<HandleTable*>(this)->find(id);
    }

    // The record of the id, which throws `std::logic_error` when the id names no live object.
    [[nodiscard]] Record& get(std::uint64_t id) {
        Record* record = find(id);
        if (record == nullptr) {
            throw std::logic_error(std::format("The {} {} does not exist.", name, id));
        }

        return *record;
    }

    // Takes the record out of its slot, or nothing when the id names no live object.
    std::optional<Record> remove(std::uint64_t id) {
        Slot* slot = findSlot(id);
        if (slot == nullptr) {
            return std::nullopt;
        }

        std::optional<Record> removed = std::exchange(slot->record, std::nullopt);
        slot->generation = (slot->generation + 1) & kGenerationMask;
        freeSlots.push_back(static_cast<std::size_t>((id & kIndexMask) - 1));
        --live;
        return removed;
    }

    // Drops every record, whose objects the caller released, and keeps the generations, so no id made before finds a record made after.
    void clear() {
        for (std::size_t index = 0; index < slots.size(); ++index) {
            Slot& slot = slots[index];
            if (slot.record) {
                slot.record.reset();
                slot.generation = (slot.generation + 1) & kGenerationMask;
                freeSlots.push_back(index);
            }
        }

        live = 0;
    }

    template <typename Visit> void forEach(Visit&& visit) {
        for (Slot& slot : slots) {
            if (slot.record) {
                visit(*slot.record);
            }
        }
    }

    [[nodiscard]] std::string_view getKind() const noexcept {
        return name;
    }

    [[nodiscard]] int size() const noexcept {
        return static_cast<int>(live);
    }

    [[nodiscard]] int capacity() const noexcept {
        return static_cast<int>(slots.size());
    }

  private:
    struct Slot {
        std::optional<Record> record;
        std::uint64_t generation = 0;
    };

    [[nodiscard]] Slot* findSlot(std::uint64_t id) noexcept {
        const std::uint64_t slotNumber = id & kIndexMask;
        if (slotNumber == 0 || slotNumber > slots.size()) {
            return nullptr;
        }

        Slot& slot = slots[static_cast<std::size_t>(slotNumber - 1)];
        return slot.record && slot.generation == id >> kIndexBits ? &slot : nullptr;
    }

    std::string_view name;
    std::deque<Slot> slots;
    std::vector<std::size_t> freeSlots;
    std::size_t live = 0;
};

} // namespace haylen::gpu
