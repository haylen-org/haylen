#pragma once

#include <string_view>

#include "haylen/math/Rect.hpp"
#include "ui/components/inputs/TextEntry.hpp"

namespace haylen::ui {

// A search field with a magnifier and a button that clears it.
class FilterField final : public TextEntry {
  public:
    [[nodiscard]] std::string_view getKind() const noexcept override {
        return "filterField";
    }

  protected:
    void render(Context& context, const math::Rect& bounds) override;
};

} // namespace haylen::ui
