#pragma once

#include <string_view>

#include "haylen/ui/Theme.hpp"
#include "ui/components/containers/Surfaced.hpp"

namespace haylen::ui {

class Card final : public Surfaced {
  public:
    Card() : Surfaced(Theme::Surface::Card, Theme::Color::Raised, true) {}
    [[nodiscard]] std::string_view getKind() const noexcept override {
        return "card";
    }
};

} // namespace haylen::ui
