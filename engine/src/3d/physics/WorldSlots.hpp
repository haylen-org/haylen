#pragma once

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <mutex>

namespace haylen::physics3d {

// The slots of the 3D worlds that live in the process, each with a generation that grows when its world ends. A key holds the slot and its generation, so the handles of a destroyed world turn invalid even after a new world takes its slot. Worlds take and give back slots under a lock, and handles read them without one.
class WorldSlots final {
  public:
    // More live worlds than a process ever needs, which keeps the table small.
    static constexpr std::size_t kMaxWorlds = 1024;

    // Takes a free slot and returns its key, which is never 0. Throws `std::runtime_error` when every slot holds a live world.
    [[nodiscard]] static std::uint64_t acquire();

    // Gives back the slot of a key, whose handles turn invalid at once.
    static void release(std::uint64_t key) noexcept;

    [[nodiscard]] static bool isLive(std::uint64_t key) noexcept;

  private:
    struct Slot {
        std::atomic<std::uint32_t> generation = 0;
        std::atomic<bool> live = false;
    };

    // The table and its lock live for the whole process, since handles may ask about a world while the process ends.
    struct Table {
        std::array<Slot, kMaxWorlds> slots;
        std::mutex mutex;
    };

    [[nodiscard]] static Table& getTable() noexcept;
};

} // namespace haylen::physics3d
