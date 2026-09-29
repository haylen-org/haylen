#pragma once

#include <string_view>

#include "ui/components/containers/Linear.hpp"

namespace haylen::ui {

class Column final : public Linear {
  public:
    Column() : Linear(false) {}
    [[nodiscard]] std::string_view getKind() const noexcept override {
        return "column";
    }
};

} // namespace haylen::ui
