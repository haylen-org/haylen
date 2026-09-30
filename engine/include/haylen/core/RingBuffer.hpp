#pragma once

#include <cstddef>
#include <iterator>
#include <optional>
#include <stdexcept>
#include <utility>
#include <vector>

namespace haylen::core {

// A queue with a fixed capacity over one buffer that never reallocates. Pushing into a full buffer drops the oldest element, which suits histories such as frame times, trails and input buffers. Index 0 is the oldest element.
template <typename T> class RingBuffer final {
  public:
    class Iterator final {
      public:
        using iterator_category = std::forward_iterator_tag;
        using value_type = T;
        using difference_type = std::ptrdiff_t;
        using pointer = const T*;
        using reference = const T&;

        Iterator(const RingBuffer* owner, std::size_t position) noexcept : buffer(owner), index(position) {}

        [[nodiscard]] reference operator*() const {
            return (*buffer)[index];
        }
        [[nodiscard]] pointer operator->() const {
            return &(*buffer)[index];
        }
        Iterator& operator++() noexcept {
            ++index;
            return *this;
        }
        Iterator operator++(int) noexcept {
            Iterator previous = *this;
            ++index;
            return previous;
        }
        [[nodiscard]] bool operator==(const Iterator&) const noexcept = default;

      private:
        const RingBuffer* buffer;
        std::size_t index;
    };

    // Throws `std::invalid_argument` when the capacity is zero.
    explicit RingBuffer(std::size_t slotCount) : slots(slotCount) {
        if (slotCount == 0) {
            throw std::invalid_argument("A ring buffer needs a capacity of at least one.");
        }
    }

    // Appends the value as the newest element and returns `true` when the oldest element had to go to make room.
    bool push(T value) {
        const bool overwrote = full();
        slots[(head + count) % slots.size()] = std::move(value);
        if (overwrote) {
            head = (head + 1) % slots.size();
        } else {
            ++count;
        }
        return overwrote;
    }

    // Removes and returns the oldest element. Throws `std::out_of_range` when the buffer is empty.
    T pop() {
        if (empty()) {
            throw std::out_of_range("The ring buffer is empty.");
        }
        T value = std::move(*slots[head]);
        slots[head].reset();
        head = (head + 1) % slots.size();
        --count;
        return value;
    }

    [[nodiscard]] const T& operator[](std::size_t index) const {
        return *slots[(head + index) % slots.size()];
    }
    [[nodiscard]] T& operator[](std::size_t index) {
        return *slots[(head + index) % slots.size()];
    }

    // Throws `std::out_of_range` when the index is past the newest element.
    [[nodiscard]] const T& at(std::size_t index) const {
        if (index >= count) {
            throw std::out_of_range("The ring buffer index is out of range.");
        }
        return (*this)[index];
    }

    [[nodiscard]] const T& front() const {
        return at(0);
    }
    [[nodiscard]] const T& back() const {
        return at(count - 1);
    }

    void clear() noexcept {
        for (std::optional<T>& slot : slots) {
            slot.reset();
        }
        head = 0;
        count = 0;
    }

    [[nodiscard]] Iterator begin() const noexcept {
        return {this, 0};
    }
    [[nodiscard]] Iterator end() const noexcept {
        return {this, count};
    }

    [[nodiscard]] std::size_t size() const noexcept {
        return count;
    }
    [[nodiscard]] std::size_t capacity() const noexcept {
        return slots.size();
    }
    [[nodiscard]] bool empty() const noexcept {
        return count == 0;
    }
    [[nodiscard]] bool full() const noexcept {
        return count == slots.size();
    }

  private:
    std::vector<std::optional<T>> slots;
    std::size_t head = 0;
    std::size_t count = 0;
};

} // namespace haylen::core
