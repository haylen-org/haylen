#pragma once

#include <cstdint>
#include <string_view>
#include <vector>

#include "haylen/core/Json.hpp"
#include "haylen/math/Rect.hpp"
#include "haylen/math/Vec2.hpp"
#include "haylen/ui/Component.hpp"
#include "haylen/ui/TextValue.hpp"
#include "haylen/ui/ToastStack.hpp"
#include "ui/Widgets.hpp"

namespace haylen::ui {

// Short notices at an edge of the safe area. The notice of the properties shows while `open` is set, the `show` command adds more, and every notice takes its place in the stack of its position, slides and fades in, and fades out after its duration while the notices after it move up.
class Toast final : public Component {
  public:
    [[nodiscard]] std::string_view getKind() const noexcept override {
        return "toast";
    }

    void command(Context& context, std::string_view name, const core::Json& arguments) override;

  protected:
    [[nodiscard]] bool isFloating() const noexcept override {
        return true;
    }
    void readProperties(PropertyReader& reader) override;
    [[nodiscard]] math::Vec2 measureContent(Context&, float) override;
    void render(Context& context, const math::Rect&) override;

  private:
    struct Notice {
        std::uint64_t key = 0;
        TextValue text;
        Widgets::Tone tone = Widgets::Tone::Information;
        float duration = 0.0F;
        bool own = false;
        double since = -1.0;
        double shownAt = -1.0;
        float appear = 0.0F;
        float offset = 0.0F;
        bool leaving = false;
    };

    // Draws one notice and returns whether it is gone.
    [[nodiscard]] bool drawNotice(Context& context, Notice& notice);
    void drawFrame(Context& context, const Notice& notice, const math::Rect& frame, const std::string& message);
    void restart();

    bool open = false;
    TextValue text;
    Widgets::Tone tone = Widgets::Tone::Information;
    float duration = 3.0F;
    ToastStack::Position position = ToastStack::Position::Top;
    std::vector<Notice> notices;
};

} // namespace haylen::ui
