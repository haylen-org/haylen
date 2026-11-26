#include "3d/physics/WorldSlots.hpp"

#include <format>
#include <stdexcept>

namespace haylen::physics3d {

WorldSlots::Table& WorldSlots::getTable() noexcept {
    static Table& table = *new Table();
    return table;
}

// A key holds the generation in its high half and the slot in its low half, and a generation starts at 1, so no key is 0.
std::uint64_t WorldSlots::acquire() {
    Table& table = getTable();
    const std::scoped_lock lock(table.mutex);
    for (std::size_t index = 0; index < kMaxWorlds; ++index) {
        Slot& slot = table.slots[index];
        if (slot.live.load(std::memory_order_relaxed)) {
            continue;
        }

        const std::uint32_t generation = slot.generation.load(std::memory_order_relaxed) + 1;
        slot.generation.store(generation, std::memory_order_relaxed);
        slot.live.store(true, std::memory_order_release);
        return (static_cast<std::uint64_t>(generation) << 32) | index;
    }

    throw std::runtime_error(std::format("The app holds {} live 3D physics worlds, the most at once. Destroy the worlds it no longer steps.", kMaxWorlds));
}

void WorldSlots::release(std::uint64_t key) noexcept {
    const auto index = static_cast<std::size_t>(key & 0xFFFFFFFFU);
    if (key == 0 || index >= kMaxWorlds) {
        return;
    }

    Table& table = getTable();
    const std::scoped_lock lock(table.mutex);
    table.slots[index].live.store(false, std::memory_order_release);
}

bool WorldSlots::isLive(std::uint64_t key) noexcept {
    const auto index = static_cast<std::size_t>(key & 0xFFFFFFFFU);
    if (key == 0 || index >= kMaxWorlds) {
        return false;
    }

    const Slot& slot = getTable().slots[index];
    return slot.live.load(std::memory_order_acquire) && slot.generation.load(std::memory_order_relaxed) == static_cast<std::uint32_t>(key >> 32);
}

} // namespace haylen::physics3d
