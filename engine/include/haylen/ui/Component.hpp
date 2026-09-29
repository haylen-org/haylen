#pragma once

#include <array>
#include <cstdint>
#include <limits>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <imgui.h>

#include "haylen/core/Json.hpp"
#include "haylen/math/Insets.hpp"
#include "haylen/math/Rect.hpp"
#include "haylen/math/Vec2.hpp"
#include "haylen/ui/Alignment.hpp"
#include "haylen/ui/Anchor.hpp"
#include "haylen/ui/FocusDirection.hpp"
#include "haylen/ui/FocusWrap.hpp"
#include "haylen/ui/PropertyReader.hpp"
#include "haylen/ui/TextValue.hpp"
#include "haylen/ui/Transform.hpp"

namespace haylen::ui {

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

        // The node ids the focus moves to from this node, in the order of FocusDirection, where an empty id leaves the choice to the search.
        std::array<std::string, 4> focusNeighbors;
    };

    // The child limit of kinds that take any number of children.
    static constexpr std::size_t kUnlimitedChildren = std::numeric_limits<std::size_t>::max();

    virtual ~Component() = default;

    Component(const Component&) = delete;
    Component& operator=(const Component&) = delete;

    // Returns where an extent of the given size starts when an alignment places it in the available span.
    [[nodiscard]] static float align(Alignment alignment, float start, float available, float size) noexcept;

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

    [[nodiscard]] math::Vec2 measure(Context& context, float availableWidth);
    void draw(Context& context, const math::Rect& layout);

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

    // Runs in the first frame a component that was drawn is no longer drawn, because it or its document was hidden, so it lets go of what it held.
    virtual void drawingStopped(Context&) {}

    // The visible children the layout of the component places, which leaves out anchored and floating children.
    [[nodiscard]] std::vector<Component*> getLayoutChildren() const;
    [[nodiscard]] bool takeFocusRequest() noexcept {
        return std::exchange(focusRequested, false);
    }

    // Takes the direction the player pressed this frame while the control had the focus, for a direction it uses itself.
    [[nodiscard]] std::optional<FocusDirection> takeFocusDirection(Context& context) const;

  private:
    friend class Document;

    static constexpr double kTooltipDelaySeconds = 0.5;
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

    [[nodiscard]] float getBoundedWidth(float width) const noexcept;
    [[nodiscard]] float getBoundedHeight(float height) const noexcept;
    void readCommon(PropertyReader& reader);
    void readFocus(PropertyReader& reader);
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
    std::uint64_t measuredFrame = 0;
    std::uint64_t drawnFrame = 0;
    float measuredWidth = -1.0F;
    math::Vec2 measuredSize;
    double hoverStarted = -1.0;
    bool focusRequested = false;
};

} // namespace haylen::ui
