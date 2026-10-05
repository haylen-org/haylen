#pragma once

#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <vector>

#include <imgui.h>

#include "haylen/core/Json.hpp"
#include "haylen/graphics/Texture.hpp"
#include "haylen/math/Color.hpp"
#include "haylen/math/Rect.hpp"
#include "haylen/math/Vec2.hpp"
#include "haylen/text/Direction.hpp"
#include "haylen/ui/Alignment.hpp"
#include "haylen/ui/Event.hpp"
#include "haylen/ui/TextValue.hpp"
#include "haylen/ui/Theme.hpp"
#include "haylen/ui/ToastStack.hpp"
#include "haylen/ui/Transform.hpp"

namespace haylen::input {
class Input;
}

namespace haylen::localization {
class Catalog;
}

namespace haylen::text {
class FontFamily;
class RichTextRegistry;
} // namespace haylen::text

namespace haylen::ui {

class Backend;
class Component;
class FocusNavigator;

// Everything a component reads while it measures and draws, so no component reaches a global or another component.
class Context final {
  public:
    // How the transforms of the nodes being drawn move, scale and color a point, composed from the outermost node in, for draws that go around the ImGui vertices of a node.
    struct Reshape {
        math::Vec2 scale{1.0F, 1.0F};
        math::Vec2 offset{};
        math::Color color = math::Color::white();
    };

    // Returns the texture of a UI image path, or an empty texture while it is still loading.
    using ImageSource = std::function<graphics::Texture(std::string_view path)>;

    // Returns the family of a UI font name, which `ui.addFont` registers and themes name, or null for a name the UI does not know.
    using FontSource = std::function<std::shared_ptr<text::FontFamily>(std::string_view name)>;

    Context(Backend& uiBackend, FocusNavigator& focusNavigator, const localization::Catalog& textCatalog, const input::Input& devices, ImageSource imageSource, FontSource fontSource, std::shared_ptr<text::RichTextRegistry> registry);

    void setTheme(const Theme& value) noexcept {
        theme = &value;
    }
    [[nodiscard]] const Theme& getTheme() const noexcept {
        return *theme;
    }
    [[nodiscard]] math::Color getColor(Theme::Color role) const noexcept {
        return theme->getColor(role);
    }
    [[nodiscard]] float getMetric(Theme::Metric role) const noexcept {
        return theme->getMetric(role);
    }
    [[nodiscard]] ImFont* getFont(Theme::Font role) const;
    [[nodiscard]] float getFontSize(Theme::Font role) const noexcept {
        return theme->getFont(role).size;
    }

    // The size of the em square of a font role, for text that draws beside ImGui at the size of its widgets, such as rich text.
    [[nodiscard]] float getEmSize(Theme::Font role) const;

    [[nodiscard]] std::string getText(const TextValue& value) const;
    [[nodiscard]] graphics::Texture getImage(std::string_view path) const;

    // Returns the family of a theme font role, or of a UI font name, which throws for a name the UI does not know.
    [[nodiscard]] std::shared_ptr<text::FontFamily> getFontFamily(Theme::Font role) const;
    [[nodiscard]] std::shared_ptr<text::FontFamily> getFontFamily(std::string_view name) const;
    [[nodiscard]] const std::shared_ptr<text::RichTextRegistry>& getTextRegistry() const noexcept {
        return textRegistry;
    }
    [[nodiscard]] ImTextureRef getTextureReference(const graphics::Texture& texture) const;

    [[nodiscard]] Backend& getBackend() const noexcept {
        return backend;
    }
    [[nodiscard]] FocusNavigator& getFocus() const noexcept {
        return focus;
    }
    [[nodiscard]] const input::Input& getInput() const noexcept {
        return input;
    }

    // On-screen controls report every frame the virtual buttons they hold and the stick values they set, and the owner releases whatever stops being reported.
    void holdButton(std::string_view name);
    void setStick(std::string_view name, math::Vec2 value);
    [[nodiscard]] const std::set<std::string, std::less<>>& getHeldButtons() const noexcept {
        return heldButtons;
    }
    [[nodiscard]] const std::map<std::string, math::Vec2, std::less<>>& getSticks() const noexcept {
        return sticks;
    }

    // The stacks the toasts of every GUI share, so toasts shown together never cover each other.
    [[nodiscard]] ToastStack& getToasts() noexcept {
        return toasts;
    }

    // Converts a point in design coordinates, such as a touch position, to UI coordinates.
    [[nodiscard]] math::Vec2 toUi(math::Vec2 designPoint) const noexcept;

    void beginFrame(double now, float delta, math::Vec2 visibleOrigin) noexcept;
    [[nodiscard]] std::uint64_t getFrame() const noexcept {
        return frame;
    }
    [[nodiscard]] double getTime() const noexcept {
        return time;
    }
    [[nodiscard]] float getDeltaSeconds() const noexcept {
        return deltaSeconds;
    }

    // Events go to the queue of the GUI being drawn.
    void setEventQueue(std::vector<Event>* value) noexcept {
        events = value;
    }
    void emit(const Component& component, std::string name, core::Json value = core::Json::object());
    void blockPointer(const math::Rect& area);

    // Nodes whose transform scales, fades or tints them push it around their drawing.
    void pushReshape(const Transform& shape, math::Vec2 center);
    void popReshape();
    [[nodiscard]] Reshape getReshape() const noexcept;

    // The direction and language of the whole UI, which nodes change for themselves and their children by pushing their own around their measuring and drawing. The direction is left to right or right to left, and the language is a BCP 47 tag or empty.
    void setBaseWriting(text::Direction direction, std::string language);
    void pushWriting(std::optional<text::Direction> direction, std::optional<std::string> language);
    void popWriting();
    [[nodiscard]] text::Direction getDirection() const noexcept;
    [[nodiscard]] bool isRightToLeft() const noexcept {
        return getDirection() == text::Direction::RightToLeft;
    }
    [[nodiscard]] const std::string& getLanguage() const noexcept;

    // Returns where a rectangle laid out inside an area from the left goes in the direction of the UI, which is its mirror image across the area when the UI reads right to left.
    [[nodiscard]] math::Rect mirror(const math::Rect& rect, const math::Rect& area) const noexcept;

    // Returns where an extent starts along the width of an area, where start and end follow the direction of the UI.
    [[nodiscard]] float alignHorizontally(Alignment alignment, float start, float available, float size) const noexcept;

  private:
    struct Writing {
        text::Direction direction = text::Direction::LeftToRight;
        std::string language;
    };

    Backend& backend;
    FocusNavigator& focus;
    const localization::Catalog& catalog;
    const input::Input& input;
    ImageSource images;
    FontSource fonts;
    std::shared_ptr<text::RichTextRegistry> textRegistry;
    const Theme* theme = nullptr;
    std::vector<Event>* events = nullptr;
    std::set<std::string, std::less<>> heldButtons;
    std::map<std::string, math::Vec2, std::less<>> sticks;
    std::vector<Reshape> reshapes;
    ToastStack toasts;

    // The direction and language in effect, from the whole UI at the bottom to the node being drawn at the top.
    std::vector<Writing> writings{Writing{}};
    math::Vec2 origin;
    std::uint64_t frame = 0;
    double time = 0.0;
    float deltaSeconds = 0.0F;
};

} // namespace haylen::ui
