#pragma once

#include <string_view>

#include "haylen/math/Rect.hpp"
#include "haylen/math/Vec2.hpp"
#include "haylen/ui/Component.hpp"

namespace haylen::ui {

// The part of a document where the game shows, as a place the focus can go. While it has the focus, the directions, accept and menu reach the action map instead of the UI, and it never takes the pointer, so clicks and touches on it reach the game too.
class PlayArea final : public Component {
  public:
    [[nodiscard]] std::string_view getKind() const noexcept override {
        return "playArea";
    }

  protected:
    [[nodiscard]] bool isFocusable() const noexcept override {
        return true;
    }
    void readProperties(PropertyReader&) override {}
    [[nodiscard]] math::Vec2 measureContent(Context&, float) override {
        return {};
    }
    void render(Context& context, const math::Rect& bounds) override;
};

} // namespace haylen::ui
