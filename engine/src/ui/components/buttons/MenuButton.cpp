#include "ui/components/buttons/MenuButton.hpp"

#include <optional>
#include <string>

#include <imgui.h>

#include "haylen/ui/Context.hpp"
#include "ui/Widgets.hpp"
#include "ui/components/ChoiceItem.hpp"
#include "ui/components/PopupList.hpp"

namespace haylen::ui {

void MenuButton::readMore(PropertyReader& reader) {
    ChoiceItem::readList(reader, "items", items);
}

void MenuButton::render(Context& context, const math::Rect& bounds) {
    if (drawButton(context, bounds)) {
        ImGui::OpenPopup("##menu");
    }
    Widgets::placePopup(context, bounds, 0.0F);
    if (const std::optional<std::string> picked = PopupList::draw(context, "##menu", items, {}, bounds.width)) {
        context.emit(*this, "select", {{"item", *picked}});
    }
}

} // namespace haylen::ui
