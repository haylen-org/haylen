#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string_view>
#include <utility>
#include <vector>

#include <imgui.h>

#include "haylen/math/Rect.hpp"
#include "haylen/ui/Message.hpp"

namespace haylen::ui {

// Stacks the messages of the navigation at the edges of the safe area, so messages that show together never cover each other. In a list a message takes its place after the messages that came on screen before it at the same position, the stack shows a limited number of each style at once, and the others wait their turn without taking room, a higher priority before a lower one and the same priority in the order they asked to show. In a deck the newest message is in front, at the edge, and the older ones peek out behind it, until the pointer or a finger opens the deck into a list.
class MessageStack final {
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

    // How the messages of a position stack: one after the other, or as a deck with the newest in front.
    enum class Arrangement : std::uint8_t {
        List,
        Deck,
    };

    // Where a message shows: its distance from the edge of the stack, how many messages of a closed deck lie in front of it, which is zero in a list and in an open deck, and the length of the front message of a closed deck, which the messages behind it peek out past.
    struct Placement {
        float offset = 0.0F;
        std::size_t depth = 0;
        float front = 0.0F;
    };

    // A message that wants to show: its key, its style, its position and arrangement, the length it takes in the stack with the space after it, when it asked to show or, once it shows, when it came on screen, its priority, whether it shows already, how many messages of its style a list shows at once, where zero shows them all, and how far each message of a closed deck lies further in than the newer one in front of it, counted past the front message. A deck that is `expandable` opens while its stack is held and lays the messages out like a list from the newest.
    struct Request {
        std::uint64_t key = 0;
        Message::Style style = Message::Style::Toast;
        Position position = Position::Bottom;
        Arrangement arrangement = Arrangement::List;
        bool expandable = true;
        float extent = 0.0F;
        double since = 0.0;
        Message::Priority priority = Message::Priority::Normal;
        bool shown = false;
        std::size_t limit = 0;
        float peek = 0.0F;
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

    // Starts a frame, in which every message that wants to show places itself again, and draws the windows of the messages of the last frame in the order of their depth, so a message behind never covers the one in front.
    void beginFrame();

    // Returns a key that names one message for as long as it lives.
    [[nodiscard]] std::uint64_t createKey() noexcept {
        return ++lastKey;
    }

    // Places a message that wants to show and returns where it shows, or nothing while it waits for room in a list. The messages that show come before those that wait, so a message keeps its place once it shows. The places follow the stack of the last frame.
    [[nodiscard]] std::optional<Placement> place(const Request& request);

    // Names the ImGui window that draws a message placed in this frame.
    void attach(std::uint64_t key, ImGuiID window) noexcept;

    // Holds the stack of a position for the next frame, while the pointer or a finger is on one of its messages or, once held, anywhere in the area its messages took in the last frame, so a deck that opens away from the pointer stays open. A held stack opens a deck and stops the time of its messages.
    void hold(Position position) noexcept;
    [[nodiscard]] bool isHeld(Position position) const noexcept;

    // Adds the frame a message drew to the area of the stack of its position in this frame.
    void cover(Position position, const math::Rect& frame) noexcept;
    [[nodiscard]] const math::Rect& getArea(Position position) const noexcept;

  private:
    struct Entry {
        std::uint64_t key = 0;
        Message::Style style = Message::Style::Toast;
        Position position = Position::Top;
        float extent = 0.0F;
        double since = 0.0;
        Message::Priority priority = Message::Priority::Normal;
        bool shown = false;

        // Whether the message found room in this frame, so it takes its length in the stack.
        bool placed = false;
        std::size_t depth = 0;
        ImGuiID window = 0;
    };

    static constexpr std::size_t kPositionCount = kPositions.size();

    // Whether an entry comes before a request in its stack: a message that shows before one that waits, among those that wait a higher priority first, and then the one that asked or came on screen first.
    [[nodiscard]] static bool isBefore(const Entry& entry, const Request& request) noexcept;

    std::vector<Entry> previous;
    std::vector<Entry> current;
    std::vector<const Entry*> order;
    std::array<bool, kPositionCount> held{};
    std::array<bool, kPositionCount> holding{};
    std::array<math::Rect, kPositionCount> areas{};
    std::array<math::Rect, kPositionCount> covering{};
    std::uint64_t lastKey = 0;
};

} // namespace haylen::ui
