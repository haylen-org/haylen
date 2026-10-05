#pragma once

#include <array>
#include <cstdint>
#include <limits>
#include <memory>
#include <optional>
#include <ranges>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <imgui.h>

#include "haylen/core/Json.hpp"
#include "haylen/math/Insets.hpp"
#include "haylen/math/Rect.hpp"
#include "haylen/math/Vec2.hpp"
#include "haylen/platform/Window.hpp"
#include "haylen/text/Direction.hpp"
#include "haylen/ui/Alignment.hpp"
#include "haylen/ui/Anchor.hpp"
#include "haylen/ui/FocusDirection.hpp"
#include "haylen/ui/FocusWrap.hpp"
#include "haylen/ui/PropertyReader.hpp"
#include "haylen/ui/Style.hpp"
#include "haylen/ui/TextValue.hpp"
#include "haylen/ui/Transform.hpp"

namespace haylen::ui {

class ComponentRegistry;
class Context;

// A node of a retained tree. Properties arrive as JSON, and the node measures, draws and reports its events on its own. Sizes are in design units.
class Component {
  public:
    struct CommonProperties {
        static constexpr float kUnbounded = std::numeric_limits<float>::max();

        bool visible = true;
        bool enabled = true;
        TextValue tooltip;
        float grow = 0.0F;
        std::optional<float> width;
        std::optional<float> height;

        // The width divided by the height, which the node keeps whatever room its parent gives it.
        std::optional<float> aspectRatio;
        float minWidth = 0.0F;
        float maxWidth = kUnbounded;
        float minHeight = 0.0F;
        float maxHeight = kUnbounded;
        std::optional<Alignment> align;
        std::optional<Anchor> anchor;
        Anchor::Area anchorArea = Anchor::Area::Safe;
        math::Insets margin;
        bool focusable = true;
        bool autofocus = false;
        bool focusScope = false;
        FocusWrap focusWrap = FocusWrap::None;

        // The node ids the focus moves to from this node, in the order of `FocusDirection`, where an empty id leaves the choice to the search.
        std::array<std::string, 4> focusNeighbors;

        // The direction and language of the node and its children, which inherit those of the node around them when unset.
        std::optional<text::Direction> direction;
        std::string language;

        // The registered theme and the style the node and its children read their colors, metrics, fonts and surfaces from, on top of the theme around them.
        std::string theme;
        std::shared_ptr<const Style> style;

        // The shape of the mouse cursor while it is over the node.
        std::optional<platform::Window::Cursor> cursor;
    };

    // The events every kind reports, each only for the nodes that listen to it, since telling them apart costs a check every frame.
    enum class Notice : std::uint8_t {
        Mount,
        Unmount,
        Show,
        Hide,
        Hover,
        Press,
        Release,
        Drag,
        Scroll,
    };

    static constexpr std::array<std::string_view, 9> kNoticeNames{"mount", "unmount", "show", "hide", "hover", "press", "release", "drag", "scroll"};

    // The child limit of kinds that take any number of children.
    static constexpr std::size_t kUnlimitedChildren = std::numeric_limits<std::size_t>::max();

    virtual ~Component() = default;

    Component(const Component&) = delete;
    Component& operator=(const Component&) = delete;

    // Returns where an extent of the given size starts when an alignment places it in the available span.
    [[nodiscard]] static float align(Alignment alignment, float start, float available, float size) noexcept;
    [[nodiscard]] static std::optional<Notice> noticeFromName(std::string_view name) noexcept;

    [[nodiscard]] virtual std::string_view getKind() const noexcept = 0;
    [[nodiscard]] const std::string& getId() const noexcept {
        return id;
    }
    [[nodiscard]] const CommonProperties& getCommon() const noexcept {
        return common;
    }

    // The number of children a kind accepts, where zero marks a leaf.
    [[nodiscard]] virtual std::size_t getChildLimit() const noexcept {
        return 0;
    }
    [[nodiscard]] std::vector<std::unique_ptr<Component>>& getChildren() noexcept {
        return children;
    }
    [[nodiscard]] const std::vector<std::unique_ptr<Component>>& getChildren() const noexcept {
        return children;
    }

    // Applies the given properties and leaves the others as they are.
    void apply(const core::Json& properties);

    // Measures the node with its margin, which a parent places around it, so the size includes the margin.
    [[nodiscard]] math::Vec2 measure(Context& context, float availableWidth);

    // Draws the node inside the rectangle its parent gives it, less its margin.
    void draw(Context& context, const math::Rect& layout);

    // Keeps a length its parent offers along one axis, margin included, within the fixed size or the minimum and maximum sizes of the node.
    [[nodiscard]] float clampWidth(float outer) const noexcept;
    [[nodiscard]] float clampHeight(float outer) const noexcept;

    // The rectangle the component was last drawn in, in UI coordinates.
    [[nodiscard]] const math::Rect& getBounds() const noexcept {
        return drawnBounds;
    }

    // The transform the node draws with, which the transform handles of Lua share so tweens animate it natively.
    [[nodiscard]] const std::shared_ptr<Transform>& getTransform() const noexcept {
        return transform;
    }

    // Returns where an anchored component goes on the screen, measured for the area its anchor names.
    [[nodiscard]] math::Rect getAnchoredBounds(Context& context);

    // A column places a child across its width with the alignment, and a row places it across its height with the row alignment.
    [[nodiscard]] Alignment getAlignment() const noexcept {
        return common.align.value_or(getDefaultAlignment());
    }
    [[nodiscard]] Alignment getRowAlignment() const noexcept {
        return common.align.value_or(getDefaultRowAlignment());
    }

    // Answers a request that is not a property, such as taking the keyboard focus.
    virtual void command(Context& context, std::string_view name, const core::Json& arguments);

    // Whether the control uses a direction itself while it has the focus, such as a slider that changes its value with left and right, so the direction does not move the focus.
    [[nodiscard]] virtual bool usesFocusDirection(FocusDirection) const noexcept {
        return false;
    }
    [[nodiscard]] bool isListening(Notice notice) const noexcept {
        return (listening & toBit(notice)) != 0U;
    }
    void setListening(Notice notice, bool value) noexcept;

  protected:
    Component() = default;

    [[nodiscard]] virtual Alignment getDefaultAlignment() const noexcept {
        return Alignment::Stretch;
    }
    [[nodiscard]] virtual Alignment getDefaultRowAlignment() const noexcept {
        return Alignment::Center;
    }
    [[nodiscard]] virtual bool isFocusable() const noexcept {
        return false;
    }

    // A floating component, such as a dialog or a window, draws over everything and takes no room in the layout that holds it.
    [[nodiscard]] virtual bool isFloating() const noexcept {
        return false;
    }
    virtual void readProperties(PropertyReader& reader) = 0;
    [[nodiscard]] virtual math::Vec2 measureContent(Context& context, float availableWidth) = 0;
    virtual void render(Context& context, const math::Rect& bounds) = 0;

    // Runs in the first frame a component that was drawn is no longer drawn, because it or its GUI was hidden, so it lets go of what it held.
    virtual void drawingStopped(Context&) {}

    // A kind that reports presses itself, such as a touch button that follows every finger, leaves out the press, release and drag every kind reports.
    [[nodiscard]] virtual bool reportsPresses() const noexcept {
        return false;
    }

    // Builds what the node needs from the registry once its properties are applied, such as the cell templates of a collection.
    virtual void compile(const ComponentRegistry&) {}

    // Whether the node prepares itself before the GUIs draw, where Lua may run, such as a collection that binds its cells.
    [[nodiscard]] virtual bool hasPreparation() const noexcept {
        return false;
    }
    virtual void prepare(Context&) {}

    // Adds the values the player changed, such as the `checked` of a toggle, under the names of their properties.
    virtual void collectPlayerValues(core::Json&) const {}

    // A floating component places itself while it renders and reports that place as the rectangle it was drawn in.
    void setBounds(const math::Rect& value) noexcept {
        drawnBounds = value;
    }

    // The visible children the layout of the component places, which leaves out anchored and floating children, as a view that allocates nothing.
    [[nodiscard]] auto getLayoutChildren() const {
        return children | std::views::filter(&Component::isPlaced) | std::views::transform(&Component::toPointer);
    }
    [[nodiscard]] bool takeFocusRequest() noexcept {
        return std::exchange(focusRequested, false);
    }

    // Takes the direction the player pressed this frame while the control had the focus, for a direction it uses itself.
    [[nodiscard]] std::optional<FocusDirection> takeFocusDirection(Context& context) const;

  private:
    friend class CellTemplate;
    friend class Collection;
    friend class Gui;

    static constexpr float kMinAspectRatio = 0.01F;
    static constexpr float kMaxAspectRatio = 100.0F;
    static constexpr std::array<std::pair<std::string_view, Alignment>, 4> kAlignments{{
        {"start", Alignment::Start},
        {"center", Alignment::Center},
        {"end", Alignment::End},
        {"stretch", Alignment::Stretch},
    }};
    static constexpr std::array<std::pair<std::string_view, Anchor::Area>, 2> kAnchorAreas{{
        {"safe", Anchor::Area::Safe},
        {"screen", Anchor::Area::Screen},
    }};
    static constexpr std::array<std::pair<std::string_view, FocusWrap>, 4> kFocusWraps{{
        {"none", FocusWrap::None},
        {"horizontal", FocusWrap::Horizontal},
        {"vertical", FocusWrap::Vertical},
        {"both", FocusWrap::Both},
    }};
    static constexpr std::array<std::string_view, 4> kNeighborKeys{"focusLeft", "focusRight", "focusUp", "focusDown"};
    static constexpr std::array<std::pair<std::string_view, std::optional<text::Direction>>, 3> kDirections{{
        {"inherit", std::nullopt},
        {"leftToRight", text::Direction::LeftToRight},
        {"rightToLeft", text::Direction::RightToLeft},
    }};

    [[nodiscard]] static math::Rect fitAspect(const math::Rect& area, float ratio) noexcept;
    [[nodiscard]] static bool isPlaced(const std::unique_ptr<Component>& child) noexcept;
    [[nodiscard]] static Component* toPointer(const std::unique_ptr<Component>& child) noexcept;
    [[nodiscard]] float getBoundedWidth(float width) const noexcept;
    [[nodiscard]] float getBoundedHeight(float height) const noexcept;
    void readCommon(PropertyReader& reader);
    void readFocus(PropertyReader& reader);
    [[nodiscard]] bool pushWriting(Context& context) const;
    [[nodiscard]] bool pushStyle(Context& context, bool drawing) const;
    [[nodiscard]] static bool isPointerOver(const math::Rect& bounds);
    [[nodiscard]] static constexpr std::uint16_t toBit(Notice notice) noexcept {
        return static_cast<std::uint16_t>(1U << static_cast<unsigned>(notice));
    }
    [[nodiscard]] bool isListeningToPointer() const noexcept {
        return (listening & (toBit(Notice::Hover) | toBit(Notice::Press) | toBit(Notice::Release) | toBit(Notice::Drag) | toBit(Notice::Scroll))) != 0U;
    }
    void reportPointer(Context& context, const math::Rect& bounds);
    void reportPress(Context& context, bool over);
    [[nodiscard]] static std::string getButtonName(int button);
    void drawTooltip(Context& context, const math::Rect& bounds);

    // Scales the vertices the node drew since the first one around the center and multiplies their colors by the tint and the opacity.
    static void reshape(int firstVertex, const Transform& shape, math::Vec2 center);
    void drawDetachedChildren(Context& context, const math::Rect& bounds);
    void noticeStoppedDrawing(Context& context);

    std::string id;
    CommonProperties common;
    std::vector<std::unique_ptr<Component>> children;
    math::Rect drawnBounds;
    std::shared_ptr<Transform> transform = std::make_shared<Transform>();
    ImGuiID drawId = 0;

    // The place of the node in the template of a collection cell, which names its ImGui items in place of its address, so they follow the item the cell shows.
    int identity = -1;
    std::uint64_t measuredFrame = 0;
    std::uint64_t drawnFrame = 0;
    float measuredWidth = -1.0F;
    math::Vec2 measuredSize;
    double hoverStarted = -1.0;
    std::uint16_t listening = 0;

    // The mouse button that pressed the node while the node listens to presses, until it lets go.
    std::optional<int> pressedButton;
    bool hovered = false;

    // Whether the node told that it joined its GUI, which it tells once.
    bool mountReported = false;
    bool focusRequested = false;
};

} // namespace haylen::ui
