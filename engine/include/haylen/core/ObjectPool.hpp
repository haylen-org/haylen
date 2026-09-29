#pragma once

#include <cstddef>
#include <cstdint>
#include <deque>
#include <limits>
#include <optional>
#include <utility>
#include <vector>

namespace haylen::core {

// Recycles objects of one type, so hot paths such as spawning projectiles stop allocating once the pool has grown to its working size. Objects never move while alive, and handles detect objects that were released, even when their slot holds a new object.
template <typename T> class ObjectPool final {
  public:
    struct Handle {
        std::uint32_t index = kInvalidIndex;
        std::uint32_t generation = 0;

        [[nodiscard]] bool operator==(const Handle&) const noexcept = default;
    };

    ObjectPool() = default;
    ObjectPool(const ObjectPool&) = delete;
    ObjectPool& operator=(const ObjectPool&) = delete;

    // Constructs an object in a free slot, growing the pool when every slot is taken.
    template <typename... Args> Handle acquire(Args&&... args) {
        if (freeSlots.empty()) {
            freeSlots.push_back(static_cast<std::uint32_t>(slots.size()));
            slots.emplace_back();
        }
        const std::uint32_t index = freeSlots.back();
        freeSlots.pop_back();
        Slot& slot = slots[index];
        slot.value.emplace(std::forward<Args>(args)...);
        ++liveCount;
        return {index, slot.generation};
    }

    // Destroys the object and frees its slot. Returns false when the handle was already released.
    bool release(Handle handle) {
        if (!isAlive(handle)) {
            return false;
        }
        Slot& slot = slots[handle.index];
        slot.value.reset();
        ++slot.generation;
        freeSlots.push_back(handle.index);
        --liveCount;
        return true;
    }

    // Returns the object, or null when the handle was released.
    [[nodiscard]] T* get(Handle handle) noexcept {
        return isAlive(handle) ? &*slots[handle.index].value : nullptr;
    }
    [[nodiscard]] const T* get(Handle handle) const noexcept {
        return isAlive(handle) ? &*slots[handle.index].value : nullptr;
    }

    [[nodiscard]] bool isAlive(Handle handle) const noexcept {
        return handle.index < slots.size() && slots[handle.index].generation == handle.generation && slots[handle.index].value.has_value();
    }

    // Grows the pool to at least count slots, so the next acquisitions do not allocate.
    void reserve(std::size_t count) {
        while (slots.size() < count) {
            freeSlots.push_back(static_cast<std::uint32_t>(slots.size()));
            slots.emplace_back();
        }
    }

    // Calls visitor with every live object and its handle, in slot order.
    template <typename Visitor> void forEach(Visitor&& visitor) {
        for (std::size_t index = 0; index < slots.size(); ++index) {
            Slot& slot = slots[index];
            if (slot.value.has_value()) {
                visitor(*slot.value, Handle{static_cast<std::uint32_t>(index), slot.generation});
            }
        }
    }

    // Releases every live object and keeps the slots for reuse.
    void clear() {
        for (std::size_t index = 0; index < slots.size(); ++index) {
            if (slots[index].value.has_value()) {
                release({static_cast<std::uint32_t>(index), slots[index].generation});
            }
        }
    }

    [[nodiscard]] std::size_t size() const noexcept {
        return liveCount;
    }
    [[nodiscard]] bool empty() const noexcept {
        return liveCount == 0;
    }
    [[nodiscard]] std::size_t getCapacity() const noexcept {
        return slots.size();
    }

  private:
    static constexpr std::uint32_t kInvalidIndex = std::numeric_limits<std::uint32_t>::max();

    struct Slot {
        std::optional<T> value;
        std::uint32_t generation = 0;
    };

    // A deque never moves its elements when it grows, so live objects keep their addresses.
    std::deque<Slot> slots;
    std::vector<std::uint32_t> freeSlots;
    std::size_t liveCount = 0;
};

} // namespace haylen::core
