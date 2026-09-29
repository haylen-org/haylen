#pragma once

#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "haylen/core/Json.hpp"
#include "haylen/math/Rect.hpp"
#include "haylen/math/Vec2.hpp"
#include "haylen/ui/Alignment.hpp"
#include "haylen/ui/Component.hpp"
#include "haylen/ui/TextValue.hpp"

namespace haylen::ui {

// Rows of cells under a header. Columns with a width keep it and the others share the rest.
class Table final : public Component {
  public:
    [[nodiscard]] std::string_view getKind() const noexcept override {
        return "table";
    }

  protected:
    void readProperties(PropertyReader& reader) override;
    [[nodiscard]] math::Vec2 measureContent(Context& context, float availableWidth) override;
    void render(Context& context, const math::Rect& bounds) override;

  private:
    struct Column {
        TextValue text;
        std::optional<float> width;
        Alignment align = Alignment::Start;
    };

    struct Row {
        std::string id;
        std::vector<TextValue> cells;
    };

    [[nodiscard]] static std::vector<Column> readColumns(PropertyReader& reader, const core::Json& value);
    [[nodiscard]] static std::vector<Row> readRows(PropertyReader& reader, const core::Json& value);
    [[nodiscard]] std::vector<float> getColumnWidths(float total) const;

    std::vector<Column> columns;
    std::vector<Row> rows;
    std::string selected;
};

} // namespace haylen::ui
