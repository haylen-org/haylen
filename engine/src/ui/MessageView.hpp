#pragma once

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "haylen/math/Color.hpp"
#include "haylen/math/Rect.hpp"
#include "haylen/math/Vec2.hpp"
#include "haylen/ui/Message.hpp"

namespace haylen::platform {
class Accessibility;
}

namespace haylen::ui {

class Backend;
class Context;

// Draws the messages of the navigation above every GUI and every presentation, each in an ImGui window of its own in the stack of its position, and answers the presses and swipes on them. A message slides in from the edge of its stack and fades in, settles into its place as the messages around it come and go, and fades out while its place shrinks, and only fades while the system asks for reduced motion. What the person did that runs code of the app waits for the next update of the navigation. Every message that comes on screen is announced to screen readers and joins the tree they read as an alert.
class MessageView final {
  public:
    MessageView(Backend& drawing, Context& drawingContext, platform::Accessibility& screenReaders);

    // Draws the messages that wait or show, in the window being built.
    void draw(const std::vector<std::shared_ptr<Message>>& messages);

  private:
    // The places of the parts of a message inside its frame.
    struct Layout {
        math::Vec2 size;
        float radius = 0.0F;
        math::Rect lead;
        math::Rect title;
        math::Rect text;
        math::Rect action;
        math::Rect close;
    };

    // The colors of a message for its variant and tone.
    struct Colors {
        math::Color fill;
        std::optional<math::Color> border;
        math::Color ink;
        math::Color accent;
        math::Color accentInk;
    };

    // A message floats low over what it covers, with this share of the shadow of dialogs and windows, so the messages of a stack never darken each other.
    static constexpr float kShadowLift = 0.4F;

    // A message behind the front of a closed deck shrinks by this share for each message in front of it.
    static constexpr float kDeckShrink = 0.05F;

    // A swipe past this share of the width of a message sends it away, and a shorter one springs back.
    static constexpr float kSwipeShare = 0.3F;
    static constexpr float kSwipeSpeed = 18.0F;

    // The picture at the start of a banner, as a share of the height of a line of its text.
    static constexpr float kIconLines = 2.0F;

    void drawMessage(Message& message);

    // Lays out the parts of a message no wider than `width`, or exactly as wide when it fills the width, as a snackbar, a banner and the messages of a deck do.
    [[nodiscard]] Layout layOut(const Message& message, float width, bool fill);
    [[nodiscard]] Colors getColors(const Message::Content& content) const;
    void drawParts(Message& message, const Layout& layout, math::Vec2 origin);
    void drawLead(const Message::Content& content, const Colors& colors, const math::Rect& area);
    void drawIndicator(const Message::Content& content, const Colors& colors, const math::Rect& area);
    void followSwipe(Message& message, float width);

    // Writes what screen readers say of a message into storage every frame reuses: its title, its text and its action.
    void describe(const Message& message);
    [[nodiscard]] static bool endsSentence(std::string_view text) noexcept;

    Backend& backend;
    Context& context;
    platform::Accessibility& accessibility;
    std::string spoken;
};

} // namespace haylen::ui
