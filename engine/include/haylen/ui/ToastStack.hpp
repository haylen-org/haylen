#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string_view>
#include <utility>
#include <vector>

#include <imgui.h>

namespace haylen::ui {

// Stacks the toasts of every GUI at the edges of the safe area, so toasts that show together never cover each other. In a list a toast takes its place after the toasts that asked to show before it at the same position, the stack shows a limited number at once, and the others wait their turn in the order they asked. In a deck the newest toast is in front, at the edge, and the older ones peek out behind it, until the pointer or a finger opens the deck into a list.
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

    // How the notices of a position stack: one after the other, or as a deck with the newest in front.
    enum class Arrangement : std::uint8_t {
        List,
        Deck,
    };

    // Where a notice shows: its distance from the edge of the stack, how many notices of a closed deck lie in front of it, which is zero in a list and in an open deck, and the length of the front notice of a closed deck, which the notices behind it peek out past.
    struct Placement {
        float offset = 0.0F;
        std::size_t depth = 0;
        float front = 0.0F;
    };

    static constexpr std::array<std::pair<std::string_view, Position>, 6> kPositions{{
        {"top", Position::Top},
        {"topStart", Position::TopStart},
        {"topEnd", Position::TopEnd},
        {"bottom", Position::Bottom},
        {"bottomStart", Position::BottomStart},
        {"bottomEnd", Position::BottomEnd},
    }};

    static constexpr std::array<std::pair<std::string_view, Arrangement>, 2> kArrangements{{
        {"list", Arrangement::List},
        {"deck", Arrangement::Deck},
    }};

    [[nodiscard]] static bool isBottom(Position position) noexcept {
        return position == Position::Bottom || position == Position::BottomStart || position == Position::BottomEnd;
    }

    // Starts a frame, in which every toast that wants to show places itself again, and draws the windows of the notices of the last frame in the order of their depth, so a notice behind never covers the one in front.
    void beginFrame();

    // Returns a key that names one notice for as long as it lives.
    [[nodiscard]] std::uint64_t createKey() noexcept {
        return ++lastKey;
    }

    // Places a notice that wants to show, which asked to at `since`, with the length it takes in the stack and the space after it, and returns where it shows, or nothing while it waits for room in a list. A list shows the first `limit` notices, and a limit of zero shows them all. A closed deck puts each notice `peek` further in than the newer one in front of it, counted past the front notice, and an open deck lays the notices out like a list from the newest. The places follow the stack of the last frame.
    [[nodiscard]] std::optional<Placement> place(std::uint64_t key, Position position, Arrangement arrangement, float extent, double since, std::size_t limit, float peek);

    // Names the ImGui window that draws a notice placed in this frame.
    void attach(std::uint64_t key, ImGuiID window) noexcept;

    // Holds the stack of a position for the next frame, while the pointer or a finger is on one of its notices, which opens a deck and stops the time of its notices.
    void hold(Position position) noexcept;
    [[nodiscard]] bool isHeld(Position position) const noexcept;

  private:
    struct Entry {
        std::uint64_t key = 0;
        Position position = Position::Top;
        float extent = 0.0F;
        double since = 0.0;
        std::size_t depth = 0;
        ImGuiID window = 0;
    };

    static constexpr std::size_t kPositionCount = kPositions.size();

    [[nodiscard]] static bool isBefore(const Entry& entry, std::uint64_t key, double since) noexcept;

    std::vector<Entry> previous;
    std::vector<Entry> current;
    std::vector<const Entry*> order;
    std::array<bool, kPositionCount> held{};
    std::array<bool, kPositionCount> holding{};
    std::uint64_t lastKey = 0;
};

} // namespace haylen::ui
