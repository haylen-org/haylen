#include "haylen/ui/Context.hpp"

#include <stdexcept>
#include <utility>

#include "haylen/localization/Catalog.hpp"
#include "haylen/text/FontFamily.hpp"
#include "haylen/text/RichTextRegistry.hpp"
#include "haylen/ui/Backend.hpp"
#include "haylen/ui/Component.hpp"

namespace haylen::ui {

Context::Context(Backend& uiBackend, FocusNavigator& focusNavigator, const localization::Catalog& textCatalog, const input::Input& devices, ImageSource imageSource, FontSource fontSource, std::shared_ptr<text::RichTextRegistry> registry) : backend(uiBackend), focus(focusNavigator), catalog(textCatalog), input(devices), images(std::move(imageSource)), fonts(std::move(fontSource)), textRegistry(std::move(registry)) {}

ImFont* Context::getFont(Theme::Font role) const {
    const Theme::FontStyle& style = theme->getFont(role);
    return backend.getFont(style.font, style.bold, style.italic);
}

float Context::getEmSize(Theme::Font role) const {
    const Theme::FontStyle& style = theme->getFont(role);
    return backend.getEmSize(style.font, style.size);
}

std::string Context::getText(const TextValue& value) const {
    return value.key.empty() ? value.literal : catalog.getText(value.key, value.arguments);
}

graphics::Texture Context::getImage(std::string_view path) const {
    return images(path);
}

std::shared_ptr<text::FontFamily> Context::getFontFamily(Theme::Font role) const {
    return getFontFamily(theme->getFont(role).font);
}

std::shared_ptr<text::FontFamily> Context::getFontFamily(std::string_view name) const {
    std::shared_ptr<text::FontFamily> family = fonts(name);
    if (!family) {
        throw std::invalid_argument("The UI has no font named " + std::string(name) + ".");
    }
    return family;
}

ImTextureRef Context::getTextureReference(const graphics::Texture& texture) const {
    return backend.getTextureReference(texture);
}

math::Vec2 Context::toUi(math::Vec2 designPoint) const noexcept {
    return designPoint - origin;
}

void Context::holdButton(std::string_view name) {
    heldButtons.emplace(name);
}

void Context::setStick(std::string_view name, math::Vec2 value) {
    sticks.insert_or_assign(std::string(name), value);
}

void Context::beginFrame(double now, float delta, math::Vec2 visibleOrigin) noexcept {
    ++frame;
    heldButtons.clear();
    sticks.clear();
    reshapes.clear();
    time = now;
    deltaSeconds = delta;
    origin = visibleOrigin;
}

void Context::emit(const Component& component, std::string name, core::Json value) {
    if (events == nullptr) {
        throw std::logic_error("A component emitted an event outside of a document.");
    }
    events->push_back({.id = component.getId(), .name = std::move(name), .value = std::move(value)});
}

void Context::blockPointer(const math::Rect& area) {
    backend.blockPointer(area);
}

// A node scales around its center, which maps a point p to p * s + center * (1 - s), and the nodes around it apply their own mapping after it.
void Context::pushReshape(const Transform& shape, math::Vec2 center) {
    const Reshape outer = getReshape();
    const math::Color color = shape.tint.withAlpha(shape.tint.a * shape.opacity);
    reshapes.push_back({.scale = shape.scale * outer.scale, .offset = center * (math::Vec2{1.0F, 1.0F} - shape.scale) * outer.scale + outer.offset, .color = color * outer.color});
}

void Context::popReshape() {
    reshapes.pop_back();
}

Context::Reshape Context::getReshape() const noexcept {
    return reshapes.empty() ? Reshape{} : reshapes.back();
}

// Nodes drawn in a new frame start from the writing of the whole UI, even when a failed script left some pushed.
void Context::setBaseWriting(text::Direction direction, std::string language) {
    writings.assign(1, {.direction = direction == text::Direction::RightToLeft ? direction : text::Direction::LeftToRight, .language = std::move(language)});
}

void Context::pushWriting(std::optional<text::Direction> direction, std::optional<std::string> language) {
    const Writing& outer = writings.back();
    writings.push_back({.direction = direction.value_or(outer.direction), .language = language ? std::move(*language) : outer.language});
}

void Context::popWriting() {
    if (writings.size() > 1) {
        writings.pop_back();
    }
}

text::Direction Context::getDirection() const noexcept {
    return writings.back().direction;
}

const std::string& Context::getLanguage() const noexcept {
    return writings.back().language;
}

math::Rect Context::mirror(const math::Rect& rect, const math::Rect& area) const noexcept {
    if (!isRightToLeft()) {
        return rect;
    }
    return {area.x + area.getRight() - rect.getRight(), rect.y, rect.width, rect.height};
}

float Context::alignHorizontally(Alignment alignment, float start, float available, float size) const noexcept {
    if (isRightToLeft() && (alignment == Alignment::Start || alignment == Alignment::End)) {
        return Component::align(alignment == Alignment::Start ? Alignment::End : Alignment::Start, start, available, size);
    }
    return Component::align(alignment, start, available, size);
}

} // namespace haylen::ui
