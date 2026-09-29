#include "ui/components/indicators/Avatar.hpp"

#include <algorithm>
#include <string>

#include <imgui.h>

#include "haylen/core/Utf8.hpp"
#include "haylen/ui/Context.hpp"
#include "ui/ImGuiConverter.hpp"
#include "ui/Typography.hpp"

namespace haylen::ui {

void Avatar::readProperties(PropertyReader& reader) {
    reader.read("image", image);
    reader.read("name", name);
    reader.read("size", size, 8.0F, 1024.0F);
}

math::Vec2 Avatar::measureContent(Context&, float) {
    return {size, size};
}

void Avatar::render(Context& context, const math::Rect& bounds) {
    const float side = std::min(bounds.width, bounds.height);
    const math::Rect circle{bounds.getCenter().x - side * 0.5F, bounds.getCenter().y - side * 0.5F, side, side};
    ImDrawList& list = *ImGui::GetWindowDrawList();
    const graphics::Texture texture = image.empty() ? graphics::Texture{} : context.getImage(image);
    if (texture.isValid()) {
        list.AddImageRounded(context.getTextureReference(texture), ImGuiConverter::toImVec2(circle.getMin()), ImGuiConverter::toImVec2(circle.getMax()), {0.0F, 0.0F}, {1.0F, 1.0F}, ImGuiConverter::toImU32(math::Color::white()), side * 0.5F);
        return;
    }
    list.AddCircleFilled(ImGuiConverter::toImVec2(circle.getCenter()), side * 0.5F, ImGuiConverter::toImU32(context.getColor(Theme::Color::AccentBackground)));
    Typography::drawAligned(context, Theme::Font::Button, circle, context.getColor(Theme::Color::AccentText), getInitials(context.getText(name)), Alignment::Center);
}

std::string Avatar::getInitials(std::string_view fullName) {
    std::string letters;
    bool start = true;
    std::size_t offset = 0;
    while (offset < fullName.size() && core::Utf8::countCodePoints(letters) < 2) {
        const char32_t character = core::Utf8::decode(fullName, offset);
        if (character == U' ') {
            start = true;
        } else if (start) {
            core::Utf8::append(letters, character);
            start = false;
        }
    }
    return letters;
}

} // namespace haylen::ui
