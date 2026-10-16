#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "haylen/core/Json.hpp"
#include "haylen/math/Rect.hpp"
#include "haylen/math/Vec2.hpp"
#include "haylen/ui/Component.hpp"
#include "haylen/ui/TextValue.hpp"
#include "haylen/ui/ToastStack.hpp"
#include "ui/Widgets.hpp"

namespace haylen::ui {

class PropertyReader;

// Short notices at an edge of the safe area. The notice of the properties shows while `open` is set, the `show` command adds more, `update` changes a notice and `hide` lets notices go. Every notice takes its place in the stack of its position, as a list or a deck, slides and fades in, stays for its duration while nothing holds its stack, and leaves when its time ends, when the player presses it, closes it, swipes it away or presses its action. A notice reports `noticeShow` when it comes on screen, `click` and `action` when the player presses it or its action, and `dismiss` with the reason when it leaves.
class Toast final : public Component {
  public:
    [[nodiscard]] std::string_view getKind() const noexcept override {
        return "toast";
    }
    [[nodiscard]] bool describe(Context& context, platform::AccessibilityElement& element) const override;

    void command(Context& context, std::string_view name, const core::Json& arguments) override;

  protected:
    [[nodiscard]] bool isFloating() const noexcept override {
        return true;
    }
    void readProperties(PropertyReader& reader) override;
    [[nodiscard]] math::Vec2 measureContent(Context&, float) override;
    void render(Context& context, const math::Rect&) override;

  private:
    // How a notice paints itself: filled with the color of its tone, filled with the pale color of its tone, or on the raised color with a border of its tone.
    enum class Variant : std::uint8_t {
        Solid,
        Soft,
        Outline,
    };

    // What a notice shows before its text: nothing, the symbol of its tone, a spinner, or a ring that fills with its progress.
    enum class Indicator : std::uint8_t {
        None,
        Icon,
        Busy,
        Progress,
    };

    // Why a notice left, which `dismiss` reports.
    enum class Reason : std::uint8_t {
        Timeout,
        Clicked,
        Closed,
        Swiped,
        Action,
        Hidden,
    };

    // What a notice says and how it looks, from the properties of the toast or the arguments of a command.
    struct Content {
        TextValue title;
        TextValue text;
        TextValue action;
        Widgets::Tone tone = Widgets::Tone::Neutral;
        Variant variant = Variant::Solid;
        Indicator indicator = Indicator::None;
        float progress = 0.0F;
        bool closable = false;
        float duration = 3.0F;
    };

    struct Notice {
        std::uint64_t key = 0;
        std::string id;
        Content content;
        bool own = false;
        double since = -1.0;
        bool shown = false;
        float elapsed = 0.0F;
        float appear = 0.0F;
        float offset = 0.0F;
        float depth = 0.0F;
        float swipe = 0.0F;
        bool swiping = false;
        bool leaving = false;
        std::optional<Reason> reason;
    };

    // The places of the parts of a notice inside its frame.
    struct Layout {
        math::Vec2 size;
        float radius = 0.0F;
        math::Rect indicator;
        math::Rect title;
        math::Rect text;
        math::Rect action;
        math::Rect close;
    };

    // The colors of a notice for its variant and tone.
    struct Colors {
        math::Color fill;
        std::optional<math::Color> border;
        math::Color ink;
        math::Color accent;
        math::Color accentInk;
    };

    static constexpr std::array<std::pair<std::string_view, Variant>, 3> kVariants{{
        {"solid", Variant::Solid},
        {"soft", Variant::Soft},
        {"outline", Variant::Outline},
    }};
    static constexpr std::array<std::pair<std::string_view, Indicator>, 4> kIndicators{{
        {"none", Indicator::None},
        {"icon", Indicator::Icon},
        {"busy", Indicator::Busy},
        {"progress", Indicator::Progress},
    }};
    static constexpr std::array<std::pair<std::string_view, Reason>, 6> kReasons{{
        {"timeout", Reason::Timeout},
        {"clicked", Reason::Clicked},
        {"closed", Reason::Closed},
        {"swiped", Reason::Swiped},
        {"action", Reason::Action},
        {"hidden", Reason::Hidden},
    }};

    // A notice behind the front of a closed deck shrinks by this share for each notice in front of it.
    static constexpr float kDeckShrink = 0.05F;

    // A swipe past this share of the width of a notice sends it away, and a shorter one springs back.
    static constexpr float kSwipeShare = 0.3F;
    static constexpr float kSwipeSpeed = 18.0F;

    static void readContent(PropertyReader& reader, Content& content);
    [[nodiscard]] static std::string_view getReasonName(Reason reason) noexcept;

    [[nodiscard]] const Content& getContent(const Notice& notice) const noexcept {
        return notice.own ? content : notice.content;
    }
    [[nodiscard]] Notice* findNotice(std::string_view id) noexcept;

    // The values of the events of a notice, which name it by its id when it has one.
    [[nodiscard]] static core::Json describeNotice(const Notice& notice);

    // Lays out the parts of a notice no wider than `width`, or exactly as wide in a deck, whose notices line up.
    [[nodiscard]] static Layout layOut(Context& context, const Content& shown, float width, bool fill);
    [[nodiscard]] static Colors getColors(Context& context, const Content& shown);

    // Draws one notice and returns whether it is gone.
    [[nodiscard]] bool drawNotice(Context& context, Notice& notice);
    void drawParts(Context& context, Notice& notice, const Layout& layout, math::Vec2 origin);
    static void drawIndicator(Context& context, const Content& shown, const Colors& colors, const math::Rect& area);
    void followSwipe(Context& context, Notice& notice, float width);
    void restart();

    bool open = false;
    Content content;
    ToastStack::Position position = ToastStack::Position::Top;
    ToastStack::Arrangement arrangement = ToastStack::Arrangement::List;
    bool expandable = true;
    std::vector<Notice> notices;
};

} // namespace haylen::ui
