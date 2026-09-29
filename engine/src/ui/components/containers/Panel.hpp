#pragma once

#include <string_view>

#include "haylen/ui/Theme.hpp"
#include "ui/components/containers/Surfaced.hpp"

namespace haylen::ui {

class Panel final : public Surfaced {
  public:
    Panel() : Surfaced(Theme::Surface::Panel, Theme::Color::Panel, false) {}
    [[nodiscard]] std::string_view getKind() const noexcept override {
        return "panel";
    }
};

} // namespace haylen::ui
