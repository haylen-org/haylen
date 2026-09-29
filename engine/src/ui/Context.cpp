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
    return backend.getFont(theme->getFont(role).font);
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

} // namespace haylen::ui
