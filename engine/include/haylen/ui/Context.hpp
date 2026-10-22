#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <deque>
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
#include "haylen/ui/AccessibilityTree.hpp"
#include "haylen/ui/Alignment.hpp"
#include "haylen/ui/Event.hpp"
#include "haylen/ui/Style.hpp"
#include "haylen/ui/TextValue.hpp"
#include "haylen/ui/Theme.hpp"
#include "haylen/ui/ToastStack.hpp"
#include "haylen/ui/Transform.hpp"

struct ImGuiWindow;

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
class Collection;
class CollectionCell;
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

        // Returns where a rectangle of the node lands on the screen.
        [[nodiscard]] math::Rect apply(const math::Rect& rect) const noexcept {
            const math::Vec2 first = rect.getMin() * scale + offset;
            const math::Vec2 second = rect.getMax() * scale + offset;
            return math::Rect::fromMinMax(math::Vec2::min(first, second), math::Vec2::max(first, second));
        }
    };

    // How the wheel or the trackpad moves an area along its axis in a frame: the distance in UI units toward its top or left end, and how the area follows it. The steps of a wheel glide there, fingers on a trackpad that reports the phases of its gestures drag the content, stretching it past an end, and a precise device without phases moves the content within its range. Fingers that touch the trackpad over the area stop its motion, and fingers that leave it let the content go on by its own motion. The momentum that the system adds once they left arrives apart, for areas that have no motion of their own.
    struct WheelScroll {
        enum class Motion : std::uint8_t {
            None,
            Glide,
            Drag,
            Free,
        };

        float distance = 0.0F;
        float momentum = 0.0F;
        Motion motion = Motion::None;
        bool touched = false;
        bool lifted = false;
    };

    // Turns the events of the components of a collection cell into events of the collection while the cell draws.
    class EventRedirect final {
      public:
        EventRedirect(Context& drawing, Collection& collection, CollectionCell& cell) noexcept;
        ~EventRedirect();

        EventRedirect(const EventRedirect&) = delete;
        EventRedirect& operator=(const EventRedirect&) = delete;

      private:
        Context& context;
        Collection* previousCollection;
        CollectionCell* previousCell;
    };

    // Returns the texture of a UI image path drawn at a scale, in pixels per unit of its natural size, which image files ignore and SVG documents rasterize for, or an empty texture while it is still loading.
    using ImageSource = std::function<graphics::Texture(std::string_view path, float scale)>;

    // Returns the natural size of a UI image path, the pixels of an image file or the size an SVG document gives itself, or zero while it is still loading.
    using ImageSizeSource = std::function<math::Vec2(std::string_view path)>;

    // Returns the family of a UI font name, which `ui.addFont` registers and themes name, or null for a name the UI does not know.
    using FontSource = std::function<std::shared_ptr<text::FontFamily>(std::string_view name)>;

    // Returns a registered theme by name, or null for a name the UI does not know.
    using ThemeSource = std::function<const Theme*(std::string_view name)>;

    // The sources a context reads images, fonts, themes and the textures of styles from.
    struct Sources {
        ImageSource images;
        ImageSizeSource imageSizes;
        FontSource fonts;
        ThemeSource themes;
        Theme::TextureLoader textures;
    };

    Context(Backend& uiBackend, FocusNavigator& focusNavigator, const localization::Catalog& textCatalog, const input::Input& devices, Sources sources, std::shared_ptr<text::RichTextRegistry> registry);

    // The active theme, which every node reads from unless a node around it gives a theme or a style of its own.
    void setTheme(const Theme& value) noexcept {
        layers.front().theme = &value;
    }
    [[nodiscard]] graphics::Texture::Filter getImageFilter() const noexcept {
        return layers.front().theme->getImageFilter();
    }

    // A node with a theme, named as registered, or a style pushes it around its measuring and drawing, so it and every node inside it read its values. A node that draws also sets the ImGui style of the parts ImGui draws, such as the opacity of disabled items.
    void pushStyle(std::string_view themeName, const Style* style, bool drawing);
    void popStyle();

    [[nodiscard]] math::Color getColor(Theme::Color role) const noexcept;
    [[nodiscard]] float getMetric(Theme::Metric role) const noexcept;
    // Returns how far the wheels and the trackpads scroll in this frame in UI units: `scrollLine` for each step of a wheel and the points a trackpad moved, with positive values for scrolling up and to the left.
    [[nodiscard]] math::Vec2 getWheelDistance() const noexcept;
    // Returns the wheel of the frame for an area that scrolls along one axis, and nothing while the gesture belongs to another area. A gesture keeps the axis it starts along and goes to the innermost area under the pointer that can move along it the way it scrolls, then stays with that area until it pauses. The `position` is how far the area shows from its top or left end, out of `maximum`, and `hovered` says whether the pointer is over the area and nothing covers it.
    [[nodiscard]] WheelScroll takeWheel(ImGuiID area, const math::Rect& bounds, bool horizontal, double position, double maximum, bool hovered);
    // Returns whether the finger that drags belongs to an area that it pressed and that can scroll along `horizontal`. The finger goes to the innermost such area once it travels past the slop along that axis more than across it, and stays with it until it lifts.
    [[nodiscard]] bool takeDrag(ImGuiID area, const math::Rect& bounds, bool horizontal);
    // Keeps the finger that holds `item` when it travels or shares the screen, for a control that follows the pointer while held, such as a slider. A finger lets go of every other item then, so a swipe or a pinch never clicks it.
    void keepDrag(ImGuiID item);
    // Returns how far an area scrolls in this frame, toward its bottom or right end when positive, while the player drags an item near one of its ends: within a quarter of its length or the theme metric `listRowHeight`, whichever is shorter, in the innermost area that can move that way, faster the closer the item gets to the edge. The `position` is how far the area shows from its top or left end, out of `maximum`.
    [[nodiscard]] float takeEdgeScroll(ImGuiID area, const math::Rect& bounds, bool horizontal, double position, double maximum);
    // Holds an item that the pointer drags outside drag and drop, such as the lifted item of a collection, for the next frame, so it scrolls the areas it nears.
    void carryItem() noexcept;
    [[nodiscard]] const Theme::Image* getSurface(Theme::Surface role) const;
    [[nodiscard]] const std::string& getFontName(Theme::Font role) const noexcept;
    // The size of the em square of a font role in UI units, which the text scale multiplies.
    [[nodiscard]] float getFontSize(Theme::Font role) const noexcept;

    // How much larger the text of the UI draws than the sizes of the theme and the styles, such as the text size the user set in the system.
    void setTextScale(float value) noexcept {
        textScale = value;
    }
    [[nodiscard]] float getTextScale() const noexcept {
        return textScale;
    }
    [[nodiscard]] bool isFontBold(Theme::Font role) const noexcept;
    [[nodiscard]] bool isFontItalic(Theme::Font role) const noexcept;
    [[nodiscard]] bool hasLigatures(Theme::Font role) const noexcept;
    [[nodiscard]] ImFont* getFont(Theme::Font role) const;

    // The height of the face of a font role from its ascent to its descent at the size of the role, which is the size ImGui draws the role at.
    [[nodiscard]] float getFontHeight(Theme::Font role) const;

    // Returns the text a value shows: its literal, or its key translated into the current language, which the context keeps until the next frame begins.
    [[nodiscard]] std::string_view getText(const TextValue& value);

    // Returns empty storage for a text that stays valid until the next frame begins, such as a translation, the bullets of a password or a shortened line, from slots that frames reuse.
    [[nodiscard]] std::string& getFrameText();

    // Returns the code points of a text, valid until the next frame begins, from slots that frames reuse.
    [[nodiscard]] std::u32string_view getFrameCodePoints(std::string_view text);

    // Returns the texture that draws a picture over `size` UI units at the density of the screen, or an empty texture while it loads.
    [[nodiscard]] graphics::Texture getImage(std::string_view path, math::Vec2 size) const;

    // Returns the texture of a picture at a scale in pixels per unit of its natural size, for draws that size it themselves, such as rich text.
    [[nodiscard]] graphics::Texture getImageAtScale(std::string_view path, float scale) const;

    // The natural size of a picture in UI units, zero while it loads.
    [[nodiscard]] math::Vec2 getImageSize(std::string_view path) const;

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
    // The elements that screen readers read, which the components add as they draw while the tree is enabled.
    [[nodiscard]] AccessibilityTree& getAccessibility() noexcept {
        return accessibility;
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

    // Converts between design coordinates, such as those of a touch, and UI coordinates.
    [[nodiscard]] math::Vec2 toUi(math::Vec2 designPoint) const noexcept;
    [[nodiscard]] math::Vec2 toDesign(math::Vec2 uiPoint) const noexcept;
    [[nodiscard]] math::Rect toDesign(const math::Rect& uiRect) const noexcept;

    void beginFrame(double now, float delta) noexcept;
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

    // A rounded surface pushes its area and corner radius around the drawing of the nodes inside it, so their highlights round concentric with its corners. Popups, dialogs and the other windows of their own never take the corners of a surface around them.
    void pushEnclosure(const math::Rect& bounds, float radius);
    void popEnclosure();
    // Returns the radii of the top-left, top-right, bottom-right and bottom-left corners of a highlight, such as a selected row: concentric with a corner of the innermost enclosure that the highlight comes within its radius of, and `radius` elsewhere.
    [[nodiscard]] std::array<float, 4> getInnerRadii(const math::Rect& bounds, float radius) const;

    // Nodes whose transform scales, fades or tints them push it around their drawing.
    void pushReshape(const Transform& shape, math::Vec2 center);
    void popReshape();

    // Starts a window of its own, a popup, a tooltip, a dialog, a toast or a window, which keeps its look whatever the transforms of the nodes around it, until `popReshape`.
    void pushPlainReshape();
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
    // A theme or a style that a node pushed, of which the values of the style come first.
    struct Layer {
        const Theme* theme = nullptr;
        const Style* style = nullptr;
        bool imgui = false;
    };

    // Returns the color of a role that the layers below the end give, from the node being drawn out to the active theme.
    [[nodiscard]] math::Color findColor(Theme::Color role, std::size_t end) const noexcept;

    // Returns the first value the layers give, from the node being drawn out to the active theme.
    template <typename Value, typename FromStyle, typename FromTheme> [[nodiscard]] Value resolve(const FromStyle& fromStyle, const FromTheme& fromTheme) const;

    struct Writing {
        text::Direction direction = text::Direction::LeftToRight;
        std::string language;
    };

    struct Enclosure {
        math::Rect bounds;
        float radius = 0.0F;
        const ImGuiWindow* window = nullptr;
    };

    Backend& backend;
    FocusNavigator& focus;
    const localization::Catalog& catalog;
    const input::Input& input;
    Sources sources;
    std::shared_ptr<text::RichTextRegistry> textRegistry;

    // The active theme at the bottom and the themes and styles the nodes being drawn push above it.
    std::vector<Layer> layers{Layer{}};
    std::vector<Event>* events = nullptr;
    Collection* redirectCollection = nullptr;
    CollectionCell* redirectCell = nullptr;
    std::set<std::string, std::less<>> heldButtons;
    std::map<std::string, math::Vec2, std::less<>> sticks;
    std::vector<Reshape> reshapes;
    std::vector<Enclosure> enclosures;
    ToastStack toasts;

    // The texts the frame keeps, in slots that frames reuse and that never move while the frame draws.
    std::deque<std::string> frameTexts;
    std::size_t frameTextCount = 0;
    std::deque<std::u32string> frameCodePoints;
    std::size_t frameCodePointCount = 0;

    // The direction and language in effect, from the whole UI at the bottom to the node being drawn at the top.
    std::vector<Writing> writings{Writing{}};
    std::uint64_t frame = 0;
    double time = 0.0;
    float deltaSeconds = 0.0F;
    float textScale = 1.0F;
    AccessibilityTree accessibility;
};

} // namespace haylen::ui
