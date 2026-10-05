#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <limits>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <imgui.h>

#include "haylen/core/Json.hpp"
#include "haylen/core/ScopedConnection.hpp"
#include "haylen/math/Insets.hpp"
#include "haylen/math/Rect.hpp"
#include "haylen/math/Vec2.hpp"
#include "haylen/ui/CollectionLayout.hpp"
#include "haylen/ui/CollectionSource.hpp"
#include "haylen/ui/Component.hpp"
#include "haylen/ui/Event.hpp"

namespace haylen::ui {

class CellTypes;
class CollectionCell;
class Scrollbar;
class Scroller;
class SizeCache;

// Shows any number of items of several types in a list or a grid that scrolls along one axis. It builds a few cells from the templates of its types, binds them to the items in view and reuses them as the player scrolls, so its cost follows the visible items, not the item count. Its one child, such as an empty state, shows while it has no items. Indices count from zero in C++ and from one in its events.
class Collection final : public Component {
  public:
    enum class Axis : std::uint8_t {
        Vertical,
        Horizontal,
    };

    enum class Arrangement : std::uint8_t {
        Linear,
        Grid,
    };

    enum class ScrollAlign : std::uint8_t {
        Nearest,
        Start,
        Center,
        End,
    };

    enum class Selection : std::uint8_t {
        None,
        Single,
        Multiple,
    };

    enum class Snap : std::uint8_t {
        None,
        Item,
        Center,
        Page,
    };

    // How a scroll to an item places it: the edge it aligns with, how far inside that edge it stays and whether the scroll is smooth.
    struct ScrollRequest {
        ScrollAlign align = ScrollAlign::Nearest;
        float offset = 0.0F;
        bool animated = true;
    };

    // Where the collection was, by item rather than by offset, so it survives changes of the items and of their sizes: the first item in view, how far its start lies from the start of the view and the item that had the focus.
    struct State {
        std::string item;
        float distance = 0.0F;
        std::string focused;
    };

    // Fills a cell after the declared bindings each time the cell receives an item, given the cell and the index of the item. Binders run before the GUIs draw, where Lua may run.
    using Binder = std::function<void(CollectionCell& cell, std::size_t index)>;

    static constexpr std::array<std::pair<std::string_view, ScrollAlign>, 4> kScrollAligns{{
        {"nearest", ScrollAlign::Nearest},
        {"start", ScrollAlign::Start},
        {"center", ScrollAlign::Center},
        {"end", ScrollAlign::End},
    }};

    Collection();
    ~Collection() override;

    [[nodiscard]] std::string_view getKind() const noexcept override {
        return "collection";
    }
    [[nodiscard]] std::size_t getChildLimit() const noexcept override {
        return 1;
    }

    // The command `focus` keeps the focus on the focused item, or gives it to the first item in view.
    void command(Context& context, std::string_view name, const core::Json& arguments) override;

    // Shows the items of a source, comparing them by id with the items shown before.
    void setSource(std::shared_ptr<CollectionSource> value);
    [[nodiscard]] const std::shared_ptr<CollectionSource>& getSource() const noexcept {
        return source;
    }

    // Places the items with a layout of the app instead of the one the `layout` property chooses.
    void setLayout(std::unique_ptr<CollectionLayout> value);

    // Registers the binder of a type, or removes it with an empty binder. Throws for a type the collection does not declare.
    void setBinder(std::string_view type, Binder binder);

    // Throws `std::invalid_argument` for an item whose type the collection does not declare, or without a type while it declares several, so a source checks its items before it changes.
    void checkType(std::string_view item, std::string_view type) const;

    [[nodiscard]] std::size_t getCount() const;
    [[nodiscard]] std::optional<std::size_t> findItem(std::string_view id) const;

    void scrollTo(std::size_t index, const ScrollRequest& request);
    void scrollBy(double delta, bool animated);
    [[nodiscard]] double getScrollOffset() const noexcept;
    void setScrollOffset(double value);
    [[nodiscard]] double getContentLength() const noexcept;
    [[nodiscard]] float getViewportLength() const noexcept;

    // Gives the focus to an item, scrolling it in with the `focusAlign` of the collection.
    void focusItem(std::size_t index);
    [[nodiscard]] std::optional<std::size_t> getFocusedIndex() const noexcept {
        return focusedIndex;
    }

    // The first and last items that showed in the view the last time the collection drew.
    [[nodiscard]] std::optional<std::pair<std::size_t, std::size_t>> getVisibleRange() const noexcept {
        return visibleRange;
    }
    [[nodiscard]] std::vector<std::size_t> getSelected() const;
    [[nodiscard]] State saveState() const;
    void restoreState(const State& value);

    // Returns the cell that shows an item, or null while the item has none.
    [[nodiscard]] CollectionCell* findCell(std::size_t index) const noexcept;

    // Whether the collection binds cells right now, while its items must not change.
    [[nodiscard]] bool isBinding() const noexcept {
        return binding;
    }

  protected:
    [[nodiscard]] bool hasPreparation() const noexcept override {
        return true;
    }
    void compile(const ComponentRegistry& components) override;
    void prepare(Context& context) override;
    void readProperties(PropertyReader& reader) override;
    [[nodiscard]] math::Vec2 measureContent(Context& context, float availableWidth) override;
    void render(Context& context, const math::Rect& bounds) override;
    void drawingStopped(Context& context) override;

  private:
    friend class Context;

    // A part of the content kept in place while the items or their sizes change: an item and how far its start lies from the start of the view, or the end of the content.
    struct Hold {
        std::size_t index = 0;
        double distance = 0.0;
        bool end = false;
    };

    // A scroll to an item that aims again every frame until the item is measured and its place stops moving, or that brings back a saved distance.
    struct PendingScroll {
        std::size_t index = 0;
        ScrollRequest request;
        std::optional<double> distance;
        double target = -1.0;
        int stableFrames = 0;
    };

    struct Range {
        std::size_t first = 0;
        std::size_t last = 0;
    };

    class BindingScope;

    static constexpr int kMaxLanes = 64;
    static constexpr int kMaxPool = 256;
    static constexpr float kMaxPrefetch = 4.0F;
    static constexpr int kStableFrames = 2;
    static constexpr float kDragSlop = 12.0F;
    static constexpr float kSnapDelay = 0.12F;
    static constexpr float kSettleDistance = 0.5F;
    static constexpr float kScrollbarFadeDelay = 0.8F;
    static constexpr float kSlideSpeed = 14.0F;
    static constexpr float kFadeSpeed = 6.0F;
    static constexpr float kScrollbarFade = 0.4F;
    static constexpr std::size_t kNotReported = std::numeric_limits<std::size_t>::max();

    static constexpr std::array<std::pair<std::string_view, Axis>, 2> kAxes{{{"vertical", Axis::Vertical}, {"horizontal", Axis::Horizontal}}};
    static constexpr std::array<std::pair<std::string_view, Arrangement>, 2> kArrangements{{{"linear", Arrangement::Linear}, {"grid", Arrangement::Grid}}};
    static constexpr std::array<std::pair<std::string_view, Selection>, 3> kSelections{{{"none", Selection::None}, {"single", Selection::Single}, {"multiple", Selection::Multiple}}};
    static constexpr std::array<std::pair<std::string_view, Snap>, 4> kSnaps{{{"none", Snap::None}, {"item", Snap::Item}, {"center", Snap::Center}, {"page", Snap::Page}}};

    [[nodiscard]] bool isVertical() const noexcept {
        return axis == Axis::Vertical;
    }
    [[nodiscard]] float getGap(const Context& context) const;
    [[nodiscard]] float getPaddingStart() const noexcept;
    [[nodiscard]] float getPaddingEnd() const noexcept;

    // Returns the room the cells give up across the axis to the lane of the scroll bar while it shows, the part of the lane that the padding on the side of the bar leaves.
    [[nodiscard]] float getLaneReserve(const Context& context) const;
    [[nodiscard]] float getMainLength() const noexcept;
    [[nodiscard]] double getBase() const noexcept;
    [[nodiscard]] double getItemStart(std::size_t index) const;
    [[nodiscard]] double getViewStart() const noexcept;
    [[nodiscard]] math::Rect getItemRect(std::size_t index) const;

    // Returns the first item of the first line that starts at or after an offset from the start of the first line.
    [[nodiscard]] std::size_t findFirstFrom(double offset) const;

    void rebuildTypes();
    [[nodiscard]] std::uint16_t readType(std::size_t index) const;
    void readTypes(std::size_t first, std::size_t count, std::vector<std::uint16_t>& found) const;
    void applyChange(const CollectionSource::Change& change);
    [[nodiscard]] std::optional<std::size_t> mapIndex(std::size_t index, const CollectionSource::Change& change) const;
    void remapIndices(const CollectionSource::Change& change, std::size_t before);
    void resolveSelection();

    void updateGeometry(Context& context, float cross);
    void ensureArranged();
    void arrange();
    [[nodiscard]] std::optional<Range> findWindow(float prefetchLength) const;
    [[nodiscard]] std::optional<std::size_t> findSticky() const;
    [[nodiscard]] std::optional<Hold> findHold() const;
    void holdView();
    void restoreHold();

    void release(const std::shared_ptr<CollectionCell>& cell);
    [[nodiscard]] bool isAnimating(const CollectionCell& cell) const noexcept;
    void noteEntering(std::size_t first, std::size_t count);
    void animate();
    void releaseOutside(const Range& window, std::optional<std::size_t> sticky);
    void bindCell(const std::shared_ptr<CollectionCell>& cell, std::size_t index, bool withBinders);
    void bindRange(std::size_t first, std::size_t last, bool withBinders);
    void runBinders();
    void measureCells(Context& context);

    void takeInput(Context& context);
    void finishDrag(Context& context);
    void takePaging(Context& context);
    void requestScroll(std::size_t index, ScrollRequest request);
    void refreshRange();
    [[nodiscard]] double findScrollTarget(const PendingScroll& pending) const;
    void resolveScroll();
    void settle();
    [[nodiscard]] double findSnapPoint() const;

    void drawCells(Context& context, const Range& window, std::optional<std::size_t> sticky);
    bool drawCell(Context& context, CollectionCell& cell, const math::Rect& layoutRect, ImGuiID reveal);
    void press(Context& context, std::size_t index);
    void select(std::size_t index, bool value);
    void drawProxy(Context& context, bool focusDrawn, ImGuiID reveal);
    void drawFaded(Context& context, CollectionCell& cell, const math::Rect& rect);
    void drawLeaving(Context& context);
    void drawEmpty(Context& context);
    void drawRefresh(Context& context);
    void report(Context& context);
    void noticeHiddenCells(Context& context);

    // Turns an event of a component of a cell into an event of the collection, after the bound values the player changed went back to the item.
    [[nodiscard]] Event toCellEvent(CollectionCell& cell, const Component& component, std::string name, core::Json value);

    Axis axis = Axis::Vertical;
    Arrangement arrangement = Arrangement::Linear;
    int lanes = 2;
    float minCellSize = 0.0F;
    float cellAspect = 0.0F;
    std::optional<float> gap;
    math::Insets padding;
    core::Json typeDefinitions;
    std::string placeholder;
    bool stickToEnd = false;
    Snap snap = Snap::None;
    bool scrollbarShown = true;
    bool laneReserved = false;
    float prefetch = 0.5F;
    int poolSize = 8;
    Selection selection = Selection::None;
    std::optional<std::vector<std::string>> requestedSelection;
    bool selectionFollowsFocus = false;
    ScrollAlign focusAlign = ScrollAlign::Nearest;
    bool rememberFocus = false;
    bool refreshable = false;
    bool refreshing = false;
    int endThreshold = 5;
    bool animateChanges = true;

    const ComponentRegistry* registry = nullptr;
    std::unique_ptr<CellTypes> types;
    bool typesChanged = true;
    std::optional<std::uint16_t> placeholderType;
    std::vector<Binder> binders;
    std::shared_ptr<CollectionSource> source;
    core::ScopedConnection connection;
    std::unique_ptr<CollectionLayout> layout;
    bool customLayout = false;
    std::optional<Arrangement> builtLayout;
    std::unique_ptr<SizeCache> sizes;
    bool estimatesDirty = true;
    float estimateFallback = 0.0F;
    std::unique_ptr<Scroller> scroller;
    std::unique_ptr<Scrollbar> scrollbar;
    std::vector<std::uint8_t> selected;
    std::vector<std::size_t> stickyItems;
    std::vector<std::uint16_t> spans;
    std::vector<std::uint16_t> typeScratch;
    std::vector<std::size_t> remapping;
    bool layoutDirty = true;

    // The cells that show items, in item order, the cells released since the last draw, and the cells whose binders still have to run with the copy they run from.
    std::vector<std::shared_ptr<CollectionCell>> cells;
    std::vector<std::shared_ptr<CollectionCell>> released;
    std::vector<std::shared_ptr<CollectionCell>> waiting;
    std::vector<std::shared_ptr<CollectionCell>> binderQueue;
    bool binding = false;

    // Cells of removed items that fade out where they were, and the ids of inserted items that fade in.
    std::vector<std::shared_ptr<CollectionCell>> leaving;
    std::vector<std::string> entering;

    math::Rect viewport;
    float crossLength = -1.0F;
    float gapLength = 0.0F;
    std::size_t laneCount = 1;
    float tallest = 0.0F;
    bool rightToLeft = false;
    std::uint64_t renderedFrame = 0;
    float delta = 0.0F;

    std::optional<Hold> hold;
    std::optional<PendingScroll> pendingScroll;
    bool pressed = false;
    bool dragging = false;
    float idle = 0.0F;
    bool moving = false;

    ImGuiID focusedTarget = 0;
    ImGuiID rememberedTarget = 0;
    std::optional<std::size_t> focusedIndex;
    std::string focusedItem;
    std::optional<std::size_t> pendingFocus;

    std::optional<std::pair<std::size_t, std::size_t>> visibleRange;
    std::size_t endReported = kNotReported;
    std::size_t startReported = kNotReported;
};

} // namespace haylen::ui
