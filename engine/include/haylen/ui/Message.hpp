#pragma once

#include <array>
#include <cstdint>
#include <memory>
#include <optional>
#include <string_view>
#include <utility>

#include "haylen/core/Json.hpp"
#include "haylen/core/Signal.hpp"

namespace haylen::ui {

class MessageView;
class Navigation;
class NavigationLink;

// A short message over everything the app shows, which tells of something that happened and goes away on its own. A toast confirms something in a pill at the bottom middle that fits its text, a snackbar tells of an action across the bottom with a button that acts on it, such as an undo after a deletion, and a banner drops in under the status bar at the top, such as a new chat message, with a picture at its start. The messages of one position stack without covering each other, as a list or as a deck. A few toasts show at once and one snackbar or banner at a time, while the others wait their turn, a higher priority before a lower one. A snackbar or a banner of a higher priority than the one that shows replaces it. A message stays for its duration while nothing holds its stack, and leaves when its time ends, when the person taps it, closes it, swipes it away or presses its action, or when the app dismisses it. Screen readers hear every message that comes on screen.
class Message final : public std::enable_shared_from_this<Message> {
  public:
    enum class Style : std::uint8_t {
        Toast,
        Snackbar,
        Banner,
    };

    enum class Priority : std::uint8_t {
        Low,
        Normal,
        High,
    };

    // Why a message left: its time ended, the person tapped it, closed it, swiped it away or pressed its action, the app dismissed it, or a message of a higher priority took its place.
    enum class Reason : std::uint8_t {
        Timeout,
        Tapped,
        Closed,
        Swiped,
        Action,
        Dismissed,
        Replaced,
    };

    // What the message says and how it looks, which the UI defines in its own header.
    struct Content;

    // The properties that change what a message says and how it looks, which `update` takes, and those that only a new message takes.
    static constexpr std::array<std::string_view, 10> kContentKeys{"title", "text", "icon", "action", "tone", "variant", "indicator", "progress", "closable", "duration"};
    static constexpr std::array<std::string_view, 5> kPlacementKeys{"style", "priority", "position", "arrangement", "expandable"};

    static constexpr std::array<std::pair<std::string_view, Style>, 3> kStyleNames{{{"toast", Style::Toast}, {"snackbar", Style::Snackbar}, {"banner", Style::Banner}}};
    static constexpr std::array<std::pair<std::string_view, Priority>, 3> kPriorityNames{{{"low", Priority::Low}, {"normal", Priority::Normal}, {"high", Priority::High}}};
    static constexpr std::array<std::pair<std::string_view, Reason>, 7> kReasonNames{{{"timeout", Reason::Timeout}, {"tapped", Reason::Tapped}, {"closed", Reason::Closed}, {"swiped", Reason::Swiped}, {"action", Reason::Action}, {"dismissed", Reason::Dismissed}, {"replaced", Reason::Replaced}}};

    // How long each style stays by default, in seconds, and a snackbar with an action, which the person needs time to reach.
    static constexpr float kToastDuration = 3.0F;
    static constexpr float kSnackbarDuration = 4.0F;
    static constexpr float kActionDuration = 6.0F;
    static constexpr float kBannerDuration = 5.0F;

    // Navigation creates messages from their properties. Throws `std::invalid_argument` for an unknown property, a wrong value and a message without a title or a text.
    explicit Message(const core::Json& properties);
    ~Message();

    Message(const Message&) = delete;
    Message& operator=(const Message&) = delete;

    // Changes what the message says and how it looks with the properties of its content, and a `duration` starts its time again. Throws `std::invalid_argument` like the constructor, and for the properties that only the constructor takes.
    void update(const core::Json& properties);

    // Lets the message go with the reason `dismissed`: a message that shows leaves the screen, and one that waits leaves the queue. A message that leaves already stays as it is.
    void dismiss();

    [[nodiscard]] Style getStyle() const noexcept {
        return style;
    }
    [[nodiscard]] Priority getPriority() const noexcept {
        return priority;
    }

    // Whether the message came on screen, which it keeps once it left.
    [[nodiscard]] bool isShown() const noexcept {
        return shownOnce;
    }

    // Whether the message leaves or left already.
    [[nodiscard]] bool isDismissed() const noexcept {
        return phase == Phase::Leaving || phase == Phase::Gone;
    }

    // Why the message leaves, nothing until it starts to.
    [[nodiscard]] const std::optional<Reason>& getReason() const noexcept {
        return reason;
    }

    [[nodiscard]] static std::string_view styleName(Style value) noexcept;
    [[nodiscard]] static std::string_view reasonName(Reason value) noexcept;

    // Tells that the message came on screen, that the person pressed its action, that the person tapped it, and that it left the screen once its reason is known.
    core::Signal<Message&> appeared;
    core::Signal<Message&> actionPressed;
    core::Signal<Message&> tapped;
    core::Signal<Message&> dismissed;

  private:
    friend class Navigation;
    friend class MessageView;
    friend class NavigationLink;

    enum class Phase : std::uint8_t {
        Waiting,
        Shown,
        Leaving,
        Gone,
    };

    // Throws for a key that a message does not know, or that only a new message takes while `creating` is not set.
    static void checkKeys(const core::Json& properties, bool creating);
    static void checkText(const Content& checked);

    // Starts to leave the screen for a reason, or leaves the queue at once when the message never showed.
    void leave(Reason why);

    // Leaves at once wherever it is, as its owner ends.
    void vanish();

    // The time a message stays: its own duration, or the one of its style.
    [[nodiscard]] float getDuration() const noexcept;

    Style style = Style::Toast;
    Priority priority = Priority::Normal;
    std::unique_ptr<Content> content;
    Phase phase = Phase::Waiting;
    std::optional<Reason> reason;
    bool shownOnce = false;

    // What the next update tells the app: that the message came on screen and why it started to leave.
    bool appearReported = false;
    bool leaveReported = false;

    // The key of the message in the stacks, when it asked to show and, once it shows, when it came on screen, and how long it stayed.
    std::uint64_t key = 0;
    double since = -1.0;
    float elapsed = 0.0F;

    // How far it came in, its distance from the edge of its stack, how many messages of a closed deck lie in front of it, and how far a swipe moved it.
    float appear = 0.0F;
    float offset = 0.0F;
    float depth = 0.0F;
    float swipe = 0.0F;
    bool swiping = false;
};

} // namespace haylen::ui
