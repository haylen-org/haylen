#pragma once

#include <cstdint>
#include <optional>

#include "haylen/math/Rect.hpp"
#include "haylen/math/Vec2.hpp"
#include "ui/components/touch/PointerTracker.hpp"

namespace haylen::ui {

class Context;

// Follows one finger that started inside the control, or the mouse when no finger is down, so several controls work at the same time on a touch screen.
class PointerTracker final {
  public:
    [[nodiscard]] std::optional<math::Vec2> update(Context& context, const math::Rect& bounds);

    [[nodiscard]] math::Vec2 getStart() const noexcept {
        return startPosition;
    }

    // Forgets the finger or mouse, so a control that shows again waits for a new press.
    void reset() noexcept;

  private:
    std::optional<std::uint64_t> touchId;
    bool mouseHeld = false;
    math::Vec2 startPosition;
};

} // namespace haylen::ui
