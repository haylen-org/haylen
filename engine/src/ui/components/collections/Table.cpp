#include "ui/components/collections/Table.hpp"

#include <algorithm>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include <imgui.h>

#include "haylen/core/JsonValidator.hpp"
#include "haylen/ui/Context.hpp"
#include "ui/ImGuiConverter.hpp"
#include "ui/Typography.hpp"
#include "ui/Widgets.hpp"
#include "ui/components/collections/ListRow.hpp"

namespace haylen::ui {

void Table::readProperties(PropertyReader& reader) {
    if (const core::Json* parsed = reader.take("columns")) {
        columns = readColumns(reader, *parsed);
    }
    if (const core::Json* parsed = reader.take("rows")) {
        rows = readRows(reader, *parsed);
    }
    reader.read("selected", selected);
}

// A table fills the width it gets, and an unbounded width, such as the one of a horizontal scroll, gives every shared column the width of the widest of them.
math::Vec2 Table::measureContent(Context& context, float availableWidth) {
    const float height = context.getMetric(Theme::Metric::ListRowHeight);
    return {availableWidth < CommonProperties::kUnbounded ? availableWidth : measureWidth(context), height * 0.75F + height * static_cast<float>(rows.size())};
}

void Table::render(Context& context, const math::Rect& bounds) {
    const float height = context.getMetric(Theme::Metric::ListRowHeight);
    const float header = height * 0.75F;
    const std::vector<float> widths = getColumnWidths(bounds.width);

    // Columns run from the right in a right-to-left UI.
    float x = bounds.x;
    for (std::size_t column = 0; column < columns.size(); ++column) {
        const math::Rect cell = context.mirror({x + ListRow::kPadding, bounds.y, widths[column] - ListRow::kPadding * 2.0F, header}, bounds);
        Typography::drawAligned(context, Theme::Font::Caption, cell, context.getColor(Theme::Color::TextMuted), context.getText(columns[column].text), columns[column].align);
        x += widths[column];
    }
    ImGui::GetWindowDrawList()->AddLine({bounds.x, bounds.y + header}, {bounds.getRight(), bounds.y + header}, ImGuiConverter::toImU32(context.getColor(Theme::Color::Border)), context.getMetric(Theme::Metric::BorderWidth));

    for (std::size_t index = 0; index < rows.size(); ++index) {
        const Row& entry = rows[index];
        const math::Rect area{bounds.x, bounds.y + header + height * static_cast<float>(index), bounds.width, height};
        ImGui::PushID(entry.id.c_str());
        const Widgets::Interaction state = ListRow::draw(context, area, entry.id == selected);
        ImGui::PopID();

        // A row scrolled out of view still takes the focus and the pointer, and only its cells are left undrawn.
        const std::size_t shown = Widgets::isVisible(context, area) ? std::min(columns.size(), entry.cells.size()) : 0;
        x = bounds.x;
        for (std::size_t column = 0; column < shown; ++column) {
            const math::Rect cell = context.mirror({x + ListRow::kPadding, area.y, widths[column] - ListRow::kPadding * 2.0F, height}, area);
            Typography::drawAligned(context, Theme::Font::Body, cell, context.getColor(Theme::Color::Text), context.getText(entry.cells[column]), columns[column].align);
            x += widths[column];
        }
        if (state.clicked) {
            selected = entry.id;
            context.emit(*this, "select", {{"item", entry.id}});
        }
    }
}

std::vector<Table::Column> Table::readColumns(PropertyReader& reader, const core::Json& value) {
    if (!PropertyReader::isList(value)) {
        reader.fail("columns", "must be a list");
    }
    std::vector<Column> parsed;
    for (const core::Json& entry : value) {
        if (!entry.is_object()) {
            reader.fail("columns", "must hold objects");
        }
        core::JsonValidator::requireKnownKeys(entry, {"text", "width", "align"}, "\"table.columns\"");
        Column column;
        if (const auto text = entry.find("text"); text != entry.end()) {
            column.text = TextValue::fromJson(*text, "table.columns.text");
        }
        if (const auto width = entry.find("width"); width != entry.end()) {
            if (!width->is_number() || !(width->get<double>() >= 0.0 && width->get<double>() <= 10000.0)) {
                reader.fail("columns", "has a width that is not a number from 0 to 10000");
            }
            column.width = width->get<float>();
        }
        if (const auto align = entry.find("align"); align != entry.end()) {
            if (!align->is_string() || (*align != "start" && *align != "center" && *align != "end")) {
                reader.fail("columns", "has an \"align\" other than \"start\", \"center\" or \"end\"");
            }
            column.align = *align == "center" ? Alignment::Center : (*align == "end" ? Alignment::End : Alignment::Start);
        }
        parsed.push_back(std::move(column));
    }
    return parsed;
}

std::vector<Table::Row> Table::readRows(PropertyReader& reader, const core::Json& value) {
    if (!PropertyReader::isList(value)) {
        reader.fail("rows", "must be a list");
    }
    std::vector<Row> parsed;
    std::set<std::string, std::less<>> ids;
    for (const core::Json& entry : value) {
        if (!entry.is_object() || !entry.contains("id") || !entry.at("id").is_string() || !entry.contains("cells") || !PropertyReader::isList(entry.at("cells"))) {
            reader.fail("rows", "must hold objects with an id and a list of cells");
        }
        core::JsonValidator::requireKnownKeys(entry, {"id", "cells"}, "\"table.rows\"");
        Row row{.id = entry.at("id").get<std::string>(), .cells = {}};
        if (!ids.insert(row.id).second) {
            reader.fail("rows", "uses the row id \"" + row.id + "\" more than once");
        }
        for (const core::Json& cell : entry.at("cells")) {
            row.cells.push_back(TextValue::fromJson(cell, "table.rows.cells"));
        }
        parsed.push_back(std::move(row));
    }
    return parsed;
}

float Table::measureWidth(Context& context) const {
    float fixed = 0.0F;
    float widest = 0.0F;
    std::size_t shared = 0;
    for (std::size_t column = 0; column < columns.size(); ++column) {
        if (columns[column].width) {
            fixed += *columns[column].width;
            continue;
        }
        ++shared;
        widest = std::max(widest, Typography::measure(context, Theme::Font::Caption, context.getText(columns[column].text)).x + ListRow::kPadding * 2.0F);
        for (const Row& row : rows) {
            if (column < row.cells.size()) {
                widest = std::max(widest, Typography::measure(context, Theme::Font::Body, context.getText(row.cells[column])).x + ListRow::kPadding * 2.0F);
            }
        }
    }
    return fixed + widest * static_cast<float>(shared);
}

std::vector<float> Table::getColumnWidths(float total) const {
    float fixed = 0.0F;
    std::size_t flexible = 0;
    for (const Column& column : columns) {
        if (column.width) {
            fixed += *column.width;
        } else {
            ++flexible;
        }
    }
    const float share = flexible > 0 ? std::max(0.0F, total - fixed) / static_cast<float>(flexible) : 0.0F;
    std::vector<float> widths;
    for (const Column& column : columns) {
        widths.push_back(column.width.value_or(share));
    }
    return widths;
}

} // namespace haylen::ui
