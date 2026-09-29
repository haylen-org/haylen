#pragma once

#include <set>
#include <string>
#include <string_view>
#include <vector>

#include "haylen/core/Json.hpp"
#include "haylen/ui/PropertyReader.hpp"
#include "haylen/ui/TextValue.hpp"

namespace haylen::ui {

// One entry of a list of choices, such as a tab, a menu entry or a list row.
struct ChoiceItem {
    std::string id;
    TextValue text;
    TextValue caption;
    std::string image;
    bool enabled = true;
    std::vector<ChoiceItem> children;

    // Reads a list of items with unique ids. Nested items are only allowed where a tree reads them.
    static void readList(PropertyReader& reader, std::string_view key, std::vector<ChoiceItem>& out, bool nested = false);
    [[nodiscard]] static int indexOf(const std::vector<ChoiceItem>& items, std::string_view itemId);

  private:
    [[nodiscard]] static ChoiceItem read(const core::Json& value, const std::string& context, bool nested, std::set<std::string, std::less<>>& ids);
};

} // namespace haylen::ui
