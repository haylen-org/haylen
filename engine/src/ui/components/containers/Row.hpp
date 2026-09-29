#pragma once

#include <string_view>

#include "ui/components/containers/Linear.hpp"

namespace haylen::ui {

class Row final : public Linear {
  public:
    Row() : Linear(true) {}
    [[nodiscard]] std::string_view getKind() const noexcept override {
        return "row";
    }
};

} // namespace haylen::ui
