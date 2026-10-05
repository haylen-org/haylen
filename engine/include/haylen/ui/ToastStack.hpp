#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string_view>
#include <utility>
#include <vector>

namespace haylen::ui {

// Stacks the toasts of every GUI at the edges of the safe area, so toasts that show together never cover each other. A toast takes its place after the toasts that asked to show before it at the same position, the stack shows a limited number at once, and the others wait their turn in the order they asked.
class ToastStack final {
  public:
    // Where a stack sits: the middle, the start or the end of the top or bottom edge of the safe area, where start and end follow the direction of the UI.
    enum class Position : std::uint8_t {
        Top,
        TopStart,
        TopEnd,
        Bottom,
        BottomStart,
        BottomEnd,
    };

    static constexpr std::array<std::pair<std::string_view, Position>, 6> kPositions{{
        {"top", Position::Top},
        {"topStart", Position::TopStart},
        {"topEnd", Position::TopEnd},
        {"bottom", Position::Bottom},
        {"bottomStart", Position::BottomStart},
        {"bottomEnd", Position::BottomEnd},
    }};

    [[nodiscard]] static bool isBottom(Position position) noexcept {
        return position == Position::Bottom || position == Position::BottomStart || position == Position::BottomEnd;
    }

    // Starts a frame, in which every toast that wants to show places itself again.
    void beginFrame() noexcept;

    // Returns a key that names one notice for as long as it lives.
    [[nodiscard]] std::uint64_t createKey() noexcept {
        return ++lastKey;
    }

    // Places a notice that wants to show, which asked to at `since`, with the length it takes in the stack and the space after it, and returns its distance from the edge of the stack, or nothing while it waits for room. The places follow the stack of the last frame.
    [[nodiscard]] std::optional<float> place(std::uint64_t key, Position position, float extent, double since, std::size_t limit);

  private:
    struct Entry {
        std::uint64_t key = 0;
        Position position = Position::Top;
        float extent = 0.0F;
        double since = 0.0;
    };

    [[nodiscard]] static bool isBefore(const Entry& entry, std::uint64_t key, double since) noexcept;

    std::vector<Entry> previous;
    std::vector<Entry> current;
    std::uint64_t lastKey = 0;
};

} // namespace haylen::ui
