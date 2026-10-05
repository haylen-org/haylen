#include "haylen/ui/Context.hpp"

#include <optional>
#include <stdexcept>
#include <utility>

#include <imgui.h>

#include "haylen/localization/Catalog.hpp"
#include "haylen/text/FontFamily.hpp"
#include "haylen/text/RichTextRegistry.hpp"
#include "haylen/ui/Backend.hpp"
#include "haylen/ui/Collection.hpp"
#include "haylen/ui/Component.hpp"
#include "ui/ImGuiConverter.hpp"

namespace haylen::ui {

Context::EventRedirect::EventRedirect(Context& drawing, Collection& collection, CollectionCell& cell) noexcept : context(drawing), previousCollection(drawing.redirectCollection), previousCell(drawing.redirectCell) {
    context.redirectCollection = &collection;
    context.redirectCell = &cell;
}

Context::EventRedirect::~EventRedirect() {
    context.redirectCollection = previousCollection;
    context.redirectCell = previousCell;
}

Context::Context(Backend& uiBackend, FocusNavigator& focusNavigator, const localization::Catalog& textCatalog, const input::Input& devices, Sources contextSources, std::shared_ptr<text::RichTextRegistry> registry) : backend(uiBackend), focus(focusNavigator), catalog(textCatalog), input(devices), sources(std::move(contextSources)), textRegistry(std::move(registry)) {}

template <typename Value, typename FromStyle, typename FromTheme> Value Context::resolve(const FromStyle& fromStyle, const FromTheme& fromTheme) const {
    for (auto layer = layers.rbegin(); layer != layers.rend(); ++layer) {
        if (layer->style != nullptr) {
            if (const auto found = fromStyle(*layer->style)) {
                return *found;
            }
        }
        if (layer->theme != nullptr) {
            return fromTheme(*layer->theme);
        }
    }
    return fromTheme(*layers.front().theme);
}

// A node that draws also pushes the colors and sizes of the scrollbars and the opacity of disabled nodes into the ImGui style, which ImGui draws with.
void Context::pushStyle(std::string_view themeName, const Style* style, bool drawing) {
    const Theme* theme = nullptr;
    if (!themeName.empty()) {
        theme = sources.themes(themeName);
        if (theme == nullptr) {
            throw std::invalid_argument("The UI has no theme named \"" + std::string(themeName) + "\".");
        }
    }
    layers.push_back({.theme = theme, .style = style, .imgui = drawing});
    if (!drawing) {
        return;
    }
    ImGui::PushStyleColor(ImGuiCol_ScrollbarGrab, ImGuiConverter::toImVec4(getColor(Theme::Color::Scrollbar)));
    ImGui::PushStyleColor(ImGuiCol_ScrollbarGrabHovered, ImGuiConverter::toImVec4(getColor(Theme::Color::ScrollbarHover)));
    ImGui::PushStyleColor(ImGuiCol_ScrollbarGrabActive, ImGuiConverter::toImVec4(getColor(Theme::Color::ScrollbarHover)));
    ImGui::PushStyleVar(ImGuiStyleVar_ScrollbarSize, getMetric(Theme::Metric::ScrollbarSize));
    ImGui::PushStyleVar(ImGuiStyleVar_ScrollbarRounding, getMetric(Theme::Metric::ScrollbarSize) * 0.5F);
    ImGui::PushStyleVar(ImGuiStyleVar_DisabledAlpha, getMetric(Theme::Metric::DisabledOpacity));
}

void Context::popStyle() {
    if (layers.size() <= 1) {
        return;
    }
    if (layers.back().imgui) {
        ImGui::PopStyleColor(3);
        ImGui::PopStyleVar(3);
    }
    layers.pop_back();
}

math::Color Context::getColor(Theme::Color role) const noexcept {
    return resolve<math::Color>([role](const Style& style) { return style.findColor(role); }, [role](const Theme& theme) { return theme.getColor(role); });
}

float Context::getMetric(Theme::Metric role) const noexcept {
    return resolve<float>([role](const Style& style) { return style.findMetric(role); }, [role](const Theme& theme) { return theme.getMetric(role); });
}

// A style that sets a surface paints it with its image, or with flat colors while the image loads or when the style asks for them.
const Theme::Image* Context::getSurface(Theme::Surface role) const {
    for (auto layer = layers.rbegin(); layer != layers.rend(); ++layer) {
        if (layer->style != nullptr && layer->style->hasSurface(role)) {
            return layer->style->findSurface(role, sources.textures);
        }
        if (layer->theme != nullptr) {
            return layer->theme->getSurface(role);
        }
    }
    return layers.front().theme->getSurface(role);
}

const std::string& Context::getFontName(Theme::Font role) const noexcept {
    return *resolve<const std::string*>([role](const Style& style) { return style.getFont(role).font ? std::optional<const std::string*>(&*style.getFont(role).font) : std::nullopt; }, [role](const Theme& theme) { return &theme.getFont(role).font; });
}

float Context::getFontSize(Theme::Font role) const noexcept {
    return resolve<float>([role](const Style& style) { return style.getFont(role).size; }, [role](const Theme& theme) { return theme.getFont(role).size; });
}

bool Context::isFontBold(Theme::Font role) const noexcept {
    return resolve<bool>([role](const Style& style) { return style.getFont(role).bold; }, [role](const Theme& theme) { return theme.getFont(role).bold; });
}

bool Context::isFontItalic(Theme::Font role) const noexcept {
    return resolve<bool>([role](const Style& style) { return style.getFont(role).italic; }, [role](const Theme& theme) { return theme.getFont(role).italic; });
}

ImFont* Context::getFont(Theme::Font role) const {
    return backend.getFont(getFontName(role), isFontBold(role), isFontItalic(role));
}

float Context::getEmSize(Theme::Font role) const {
    return backend.getEmSize(getFontName(role), getFontSize(role));
}

std::string Context::getText(const TextValue& value) const {
    return value.key.empty() ? value.literal : catalog.getText(value.key, value.arguments);
}

graphics::Texture Context::getImage(std::string_view path) const {
    return sources.images(path);
}

std::shared_ptr<text::FontFamily> Context::getFontFamily(Theme::Font role) const {
    return getFontFamily(getFontName(role));
}

std::shared_ptr<text::FontFamily> Context::getFontFamily(std::string_view name) const {
    std::shared_ptr<text::FontFamily> family = sources.fonts(name);
    if (!family) {
        throw std::invalid_argument("The UI has no font named \"" + std::string(name) + "\".");
    }
    return family;
}

ImTextureRef Context::getTextureReference(const graphics::Texture& texture) const {
    return backend.getTextureReference(texture);
}

math::Vec2 Context::toUi(math::Vec2 designPoint) const noexcept {
    return backend.toUi(designPoint);
}

math::Vec2 Context::toDesign(math::Vec2 uiPoint) const noexcept {
    return backend.toDesign(uiPoint);
}

math::Rect Context::toDesign(const math::Rect& uiRect) const noexcept {
    return math::Rect::fromMinMax(backend.toDesign(uiRect.getMin()), backend.toDesign(uiRect.getMax()));
}

void Context::holdButton(std::string_view name) {
    heldButtons.emplace(name);
}

void Context::setStick(std::string_view name, math::Vec2 value) {
    sticks.insert_or_assign(std::string(name), value);
}

void Context::beginFrame(double now, float delta) noexcept {
    ++frame;
    heldButtons.clear();
    sticks.clear();
    reshapes.clear();
    layers.resize(1);
    toasts.beginFrame();
    time = now;
    deltaSeconds = delta;
}

void Context::emit(const Component& component, std::string name, core::Json value) {
    if (events == nullptr) {
        throw std::logic_error("A component emitted an event outside of a GUI.");
    }
    if (redirectCell != nullptr) {
        events->push_back(redirectCollection->toCellEvent(*redirectCell, component, std::move(name), std::move(value)));
        return;
    }
    events->push_back({.id = component.getId(), .name = std::move(name), .value = std::move(value)});
}

void Context::blockPointer(const math::Rect& area) {
    backend.blockPointer(area);
}

// A node scales around its center, which maps a point `p` to `p * s + center * (1 - s)`, and the nodes around it apply their own mapping after it.
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
