#include "haylen/ui/Collection.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <utility>

#include <imgui.h>
#include <imgui_internal.h>

#include "haylen/core/JsonNumber.hpp"
#include "haylen/input/Input.hpp"
#include "haylen/ui/CollectionCell.hpp"
#include "haylen/ui/Context.hpp"
#include "haylen/ui/FocusNavigator.hpp"
#include "haylen/ui/PropertyReader.hpp"
#include "ui/ImGuiConverter.hpp"
#include "ui/Scrollbar.hpp"
#include "ui/Scroller.hpp"
#include "ui/Surfaces.hpp"
#include "ui/Widgets.hpp"
#include "ui/components/collections/CellTemplate.hpp"
#include "ui/components/collections/CellTypes.hpp"
#include "ui/components/collections/CollectionDiff.hpp"
#include "ui/components/collections/GridLayout.hpp"
#include "ui/components/collections/LinearLayout.hpp"
#include "ui/components/collections/SizeCache.hpp"

namespace haylen::ui {

// Marks the collection as binding while it lives, also when a binder fails.
class Collection::BindingScope final {
  public:
    explicit BindingScope(bool& flag) noexcept : binding(flag) {
        binding = true;
    }
    ~BindingScope() {
        binding = false;
    }

    BindingScope(const BindingScope&) = delete;
    BindingScope& operator=(const BindingScope&) = delete;

  private:
    bool& binding;
};

Collection::Collection() : layout(std::make_unique<LinearLayout>()), sizes(std::make_unique<SizeCache>()), scroller(std::make_unique<Scroller>()), scrollbar(std::make_unique<Scrollbar>()) {}

Collection::~Collection() = default;

void Collection::readProperties(PropertyReader& reader) {
    for (const std::string_view key : {"axis", "layout", "lanes", "minCellSize", "cellAspect", "gap", "padding", "types", "placeholder"}) {
        layoutDirty = layoutDirty || reader.has(key);
    }
    typesChanged = typesChanged || reader.has("types") || reader.has("placeholder") || reader.has("layout");
    reader.readChoice<Axis>("axis", axis, kAxes);
    reader.readChoice<Arrangement>("layout", arrangement, kArrangements);
    reader.read("lanes", lanes, 1, kMaxLanes);
    reader.read("minCellSize", minCellSize, 0.0F, 10000.0F);
    reader.read("cellAspect", cellAspect, 0.0F, 100.0F);
    if (reader.has("gap")) {
        float value = 0.0F;
        reader.read("gap", value, 0.0F, 10000.0F);
        gap = value;
    }
    reader.read("padding", padding);
    if (const core::Json* definitions = reader.take("types")) {
        typeDefinitions = *definitions;
    }
    reader.read("placeholder", placeholder);
    reader.read("stickToEnd", stickToEnd);
    reader.readChoice<Snap>("snap", snap, kSnaps);
    reader.read("scrollbar", scrollbarShown);
    reader.read("prefetch", prefetch, 0.0F, kMaxPrefetch);
    reader.read("poolSize", poolSize, 0, kMaxPool);
    reader.readChoice<Selection>("selection", selection, kSelections);
    if (const core::Json* chosen = reader.take("selected")) {
        if (!PropertyReader::isList(*chosen) || !std::ranges::all_of(*chosen, &core::Json::is_string)) {
            reader.fail("selected", "must be a list of item ids");
        }
        requestedSelection.emplace();
        for (const core::Json& id : *chosen) {
            requestedSelection->push_back(id.get<std::string>());
        }
    }
    reader.read("selectionFollowsFocus", selectionFollowsFocus);
    reader.readChoice<ScrollAlign>("focusAlign", focusAlign, kScrollAligns);
    reader.read("rememberFocus", rememberFocus);
    reader.read("refreshable", refreshable);
    reader.read("refreshing", refreshing);
    reader.read("endThreshold", endThreshold, 0, 10000);
    reader.read("animateChanges", animateChanges);
}

void Collection::compile(const ComponentRegistry& components) {
    registry = &components;
    if (!binding) {
        rebuildTypes();
    }
}

// New types replace every cell, since a cell belongs to the template it was built from, and keep the binders of the types that stay.
void Collection::rebuildTypes() {
    if (!typesChanged) {
        return;
    }
    if (typeDefinitions.is_null()) {
        throw std::invalid_argument("A \"collection\" needs at least one type in \"types\".");
    }
    auto built = std::make_unique<CellTypes>(*registry, typeDefinitions);
    std::optional<std::uint16_t> placeholderIndex;
    if (!placeholder.empty()) {
        placeholderIndex = built->find(placeholder);
        if (!placeholderIndex) {
            throw std::invalid_argument("The property \"placeholder\" of a \"collection\" names the type \"" + placeholder + "\", which it does not declare.");
        }
    }
    for (std::uint16_t type = 0; type < built->size(); ++type) {
        const CellTemplate& definition = built->get(type);
        if (arrangement == Arrangement::Grid && definition.isSticky() && definition.getSpan() != 0) {
            throw std::invalid_argument("The sticky type \"" + definition.getName() + "\" of a grid collection must span the whole line.");
        }
    }

    std::vector<Binder> kept(built->size());
    for (std::uint16_t type = 0; types && type < types->size(); ++type) {
        if (const std::optional<std::uint16_t> found = built->find(types->get(type).getName())) {
            kept[*found] = std::move(binders[type]);
        }
    }
    cells.clear();
    released.clear();
    waiting.clear();
    leaving.clear();
    binders = std::move(kept);
    types = std::move(built);
    placeholderType = placeholderIndex;
    typesChanged = false;
    estimatesDirty = true;
    layoutDirty = true;
    if (source) {
        readTypes(0, source->getCount(), typeScratch);
        sizes->assign(typeScratch);
    }
}

void Collection::checkType(std::string_view item, std::string_view type) const {
    if (type.empty() && types->size() != 1) {
        throw std::invalid_argument("The item \"" + std::string(item) + "\" of the collection \"" + getId() + "\" needs a type, because the collection declares several.");
    }
    if (!type.empty() && !types->find(type)) {
        throw std::invalid_argument("The item \"" + std::string(item) + "\" of the collection \"" + getId() + "\" has the type \"" + std::string(type) + "\", which the collection does not declare.");
    }
}

// An item that did not load yet takes the placeholder type, or no type, which draws nothing in its place.
std::uint16_t Collection::readType(std::size_t index) const {
    const std::string_view id = source->getId(index);
    if (id.empty()) {
        return placeholderType.value_or(SizeCache::kNoType);
    }
    const std::string_view name = source->getType(index);
    checkType(id, name);
    return name.empty() ? 0 : *types->find(name);
}

void Collection::readTypes(std::size_t first, std::size_t count, std::vector<std::uint16_t>& found) const {
    found.resize(count);
    for (std::size_t offset = 0; offset < count; ++offset) {
        found[offset] = readType(first + offset);
    }
}

void Collection::setSource(std::shared_ptr<CollectionSource> value) {
    if (binding) {
        throw std::logic_error("The collection \"" + getId() + "\" cannot change its items while it binds cells.");
    }
    std::vector<std::string_view> before;
    if (source) {
        before.reserve(source->getCount());
        for (std::size_t index = 0; index < source->getCount(); ++index) {
            before.push_back(source->getId(index));
        }
    }
    std::vector<std::string_view> after;
    after.reserve(value->getCount());
    for (std::size_t index = 0; index < value->getCount(); ++index) {
        after.push_back(value->getId(index));
    }
    const CollectionDiff::Result result = CollectionDiff::compare(before, after, getId());

    const std::shared_ptr<CollectionSource> previous = std::exchange(source, std::move(value));
    connection = source->changed.connect([this](const CollectionSource::Change& change) { applyChange(change); });
    applyChange({.kind = CollectionSource::Change::Kind::Replaced, .index = 0, .count = source->getCount(), .target = 0, .previous = result.previous});
}

// Every change applies at once, so the collection, its sizes, its selection and its focus always match the source, and the view keeps the item it showed in place.
void Collection::applyChange(const CollectionSource::Change& change) {
    using Kind = CollectionSource::Change::Kind;
    if (binding) {
        throw std::logic_error("The collection \"" + getId() + "\" cannot change its items while it binds cells.");
    }
    holdView();
    for (const std::shared_ptr<CollectionCell>& cell : cells) {
        if (isAnimating(*cell) && !cell->origin) {
            cell->origin = cell->placed.getMin();
        }
    }
    const std::size_t before = sizes->size();
    const auto first = static_cast<std::ptrdiff_t>(change.index);
    const auto count = static_cast<std::ptrdiff_t>(change.count);
    switch (change.kind) {
    case Kind::Inserted:
        readTypes(change.index, change.count, typeScratch);
        sizes->insert(change.index, typeScratch);
        selected.insert(selected.begin() + first, change.count, 0);
        noteEntering(change.index, change.count);
        break;
    case Kind::Removed:
        sizes->erase(change.index, change.count);
        selected.erase(selected.begin() + first, selected.begin() + first + count);
        break;
    case Kind::Moved:
        sizes->move(change.index, change.target);
        if (change.index < change.target) {
            std::rotate(selected.begin() + first, selected.begin() + first + 1, selected.begin() + static_cast<std::ptrdiff_t>(change.target) + 1);
        } else {
            std::rotate(selected.begin() + static_cast<std::ptrdiff_t>(change.target), selected.begin() + first, selected.begin() + first + 1);
        }
        break;
    case Kind::Changed:
        for (std::size_t index = change.index; index < change.index + change.count; ++index) {
            sizes->setType(index, readType(index));
        }
        break;
    case Kind::Replaced: {
        readTypes(0, change.previous.size(), typeScratch);
        sizes->remap(change.previous, typeScratch);
        std::vector<std::uint8_t> next(change.previous.size(), 0);
        remapping.assign(before, CollectionSource::Change::kNew);
        for (std::size_t index = 0; index < change.previous.size(); ++index) {
            if (change.previous[index] != CollectionSource::Change::kNew) {
                next[index] = selected[change.previous[index]];
                remapping[change.previous[index]] = index;
            }
        }
        selected = std::move(next);
        for (std::size_t index = 0; index < change.previous.size(); ++index) {
            if (change.previous[index] == CollectionSource::Change::kNew) {
                noteEntering(index, 1);
            }
        }
        break;
    }
    }
    remapIndices(change, before);
    layoutDirty = true;
}

std::optional<std::size_t> Collection::mapIndex(std::size_t index, const CollectionSource::Change& change) const {
    using Kind = CollectionSource::Change::Kind;
    switch (change.kind) {
    case Kind::Inserted:
        return index >= change.index ? index + change.count : index;
    case Kind::Removed:
        if (index < change.index) {
            return index;
        }
        if (index >= change.index + change.count) {
            return index - change.count;
        }
        return std::nullopt;
    case Kind::Moved:
        if (index == change.index) {
            return change.target;
        }
        if (change.index < change.target && index > change.index && index <= change.target) {
            return index - 1;
        }
        if (change.target < change.index && index >= change.target && index < change.index) {
            return index + 1;
        }
        return index;
    case Kind::Changed:
        return index;
    case Kind::Replaced:
        return index < remapping.size() && remapping[index] != CollectionSource::Change::kNew ? std::optional<std::size_t>(remapping[index]) : std::nullopt;
    }
    return index;
}

// Cells follow their items, and a cell whose item left or changed its type goes back to its pool. The focus of a removed item passes to the item that takes its place.
void Collection::remapIndices(const CollectionSource::Change& change, std::size_t before) {
    using Kind = CollectionSource::Change::Kind;
    const std::size_t count = sizes->size();
    // clang-format off
    std::erase_if(cells, [&](const std::shared_ptr<CollectionCell>& cell) {
        const std::optional<std::size_t> index = mapIndex(cell->index, change);
        if (!index || sizes->getType(*index) == SizeCache::kNoType || &types->get(sizes->getType(*index)) != &cell->type) {
            if (cell->origin && cell->placed.intersects(viewport)) {
                cell->index = CollectionCell::kNoIndex;
                cell->waiting = false;
                ++cell->generation;
                leaving.push_back(cell);
            } else {
                release(cell);
            }
            return true;
        }
        const bool changedHere = change.kind == Kind::Changed && cell->index >= change.index && cell->index < change.index + change.count;
        cell->stale = cell->stale || changedHere || change.kind == Kind::Replaced;
        if (*index != cell->index) {
            cell->type.bindIndex(*cell, *index);
        }
        return false;
    });
    // clang-format on
    std::ranges::sort(cells, {}, [](const std::shared_ptr<CollectionCell>& cell) { return cell->index; });

    if (pendingFocus) {
        pendingFocus = mapIndex(*pendingFocus, change);
    }
    if (pendingScroll) {
        const std::optional<std::size_t> index = mapIndex(pendingScroll->index, change);
        if (index) {
            pendingScroll->index = *index;
        } else {
            pendingScroll.reset();
        }
    }
    // A held item that moves leaves its place to the item that takes it, so the view stays where it was.
    if (hold && !hold->end) {
        std::optional<std::size_t> index;
        if (change.kind == Kind::Moved && hold->index == change.index) {
            index = hold->index;
        }
        for (std::size_t next = hold->index; !index && next < before; ++next) {
            index = mapIndex(next, change);
        }
        if (index) {
            hold->index = *index;
        } else {
            hold.reset();
        }
    }
    if (focusedIndex) {
        const std::size_t old = *focusedIndex;
        focusedIndex = mapIndex(old, change);
        if (!focusedIndex && count > 0) {
            focusItem(std::min(change.kind == Kind::Removed ? change.index : old, count - 1));
        }
    }
}

// A selection the app sets replaces the one the player made, matched by id in one pass over the items.
void Collection::resolveSelection() {
    if (!requestedSelection || !source) {
        return;
    }
    std::vector<std::string> ids = std::move(*requestedSelection);
    requestedSelection.reset();
    std::ranges::sort(ids);
    for (std::size_t index = 0; index < selected.size(); ++index) {
        select(index, std::ranges::binary_search(ids, source->getId(index)));
    }
}

float Collection::getGap(const Context& context) const {
    return gap.value_or(context.getMetric(Theme::Metric::ItemSpacing));
}

float Collection::getPaddingStart() const noexcept {
    if (isVertical()) {
        return padding.top;
    }
    return rightToLeft ? padding.right : padding.left;
}

float Collection::getPaddingEnd() const noexcept {
    if (isVertical()) {
        return padding.bottom;
    }
    return rightToLeft ? padding.left : padding.right;
}

float Collection::getLaneReserve(const Context& context) const {
    if (!laneReserved) {
        return 0.0F;
    }
    const float edge = isVertical() ? (context.isRightToLeft() ? padding.left : padding.right) : padding.bottom;
    return Scrollbar::getReserve(context, edge);
}

float Collection::getMainLength() const noexcept {
    return isVertical() ? viewport.height : viewport.width;
}

std::size_t Collection::getCount() const {
    return sizes->size();
}

double Collection::getContentLength() const noexcept {
    return sizes->size() == 0 ? 0.0 : getPaddingStart() + sizes->getTotal() + getPaddingEnd();
}

float Collection::getViewportLength() const noexcept {
    return getMainLength();
}

// Short content of a collection that sticks to its end lies against the end of the view, as the newest messages of a chat do.
double Collection::getBase() const noexcept {
    return stickToEnd ? std::max(0.0, static_cast<double>(getMainLength()) - getContentLength()) : 0.0;
}

double Collection::getItemStart(std::size_t index) const {
    return getPaddingStart() + sizes->getLineOffset(layout->getLine(index));
}

double Collection::getViewStart() const noexcept {
    return scroller->getOffset() - getBase() - getPaddingStart();
}

void Collection::updateGeometry(Context& context, float cross) {
    rebuildTypes();
    const float fallback = context.getMetric(Theme::Metric::ListRowHeight);
    if (estimatesDirty || fallback != estimateFallback) {
        std::vector<float> declared(types->size());
        for (std::uint16_t type = 0; type < types->size(); ++type) {
            declared[type] = types->get(type).getEstimatedSize();
        }
        sizes->setEstimates(declared, fallback);
        estimateFallback = fallback;
        estimatesDirty = false;
    }
    const float spacing = getGap(context);
    if (cross != crossLength || spacing != gapLength) {
        crossLength = cross;
        gapLength = spacing;
        sizes->markStale();
        layoutDirty = true;
    }
    ensureArranged();
}

void Collection::ensureArranged() {
    if (layoutDirty || sizes->needsArrange()) {
        arrange();
    }
}

// Grid cells that do not span the whole line take their length from the aspect ratio when it is set, so they need no measuring.
void Collection::arrange() {
    if (!customLayout && builtLayout != arrangement) {
        layout = arrangement == Arrangement::Grid ? std::unique_ptr<CollectionLayout>(std::make_unique<GridLayout>()) : std::make_unique<LinearLayout>();
        builtLayout = arrangement;
    }
    const std::size_t count = sizes->size();
    const float cross = std::max(0.0F, crossLength);
    laneCount = 1;
    if (arrangement == Arrangement::Grid) {
        laneCount = minCellSize > 0.0F ? GridLayout::fitLanes(cross, minCellSize, gapLength) : static_cast<std::size_t>(lanes);
    }
    const auto lanesAcross = static_cast<float>(laneCount);
    const float laneLength = std::max(0.0F, (cross - gapLength * (lanesAcross - 1.0F)) / lanesAcross);

    bool spanned = false;
    for (std::uint16_t type = 0; type < types->size(); ++type) {
        const std::uint16_t span = types->get(type).getSpan();
        spanned = spanned || (arrangement == Arrangement::Grid && span != 1);
        std::optional<float> fixed;
        if (arrangement == Arrangement::Grid && cellAspect > 0.0F && span != 0) {
            const auto spanLanes = static_cast<float>(std::min<std::size_t>(span, laneCount));
            fixed = (laneLength * spanLanes + gapLength * (spanLanes - 1.0F)) * cellAspect;
        }
        sizes->setFixedLength(type, fixed);
    }
    spans.clear();
    stickyItems.clear();
    if (spanned) {
        spans.resize(count, 1);
    }
    for (std::size_t index = 0; index < count; ++index) {
        const std::uint16_t type = sizes->getType(index);
        if (type == SizeCache::kNoType) {
            continue;
        }
        if (spanned) {
            spans[index] = types->get(type).getSpan();
        }
        if (types->get(type).isSticky()) {
            stickyItems.push_back(index);
        }
    }
    layout->arrange({.count = count, .crossLength = cross, .gap = gapLength, .lanes = laneCount, .spans = spans}, 0);
    sizes->arrange(*layout, gapLength);
    layoutDirty = false;
    scroller->setMaximum(getContentLength() - getMainLength());
}

std::size_t Collection::findFirstFrom(double offset) const {
    std::size_t line = sizes->findLine(offset);
    if (sizes->getLineOffset(line) < offset - kSettleDistance && line + 1 < sizes->getLineCount()) {
        ++line;
    }
    return layout->getFirstIndex(line);
}

// Vertical cells take the width of their slot, and horizontal ones lie along the axis from the start of the reading direction, with the lanes of a vertical grid filled from the start side too.
math::Rect Collection::getItemRect(std::size_t index) const {
    const std::size_t line = layout->getLine(index);
    const auto main = static_cast<float>(getBase() + getPaddingStart() + sizes->getLineOffset(line) - scroller->getOffset());
    const float length = sizes->getLineLength(line);
    const CollectionLayout::Slot slot = layout->getSlot(index);
    if (isVertical()) {
        const float cross = rightToLeft ? viewport.getRight() - padding.right - slot.crossStart - slot.crossLength : viewport.x + padding.left + slot.crossStart;
        return {std::floor(cross), std::floor(viewport.y + main), slot.crossLength, length};
    }
    const float along = rightToLeft ? viewport.getRight() - main - length : viewport.x + main;
    return {std::floor(along), std::floor(viewport.y + padding.top + slot.crossStart), length, slot.crossLength};
}

// The window holds the lines in view and one more line on each side, so the next item is always a real focus target, the lines around the focused item while it lies within a view of them, as it does while a scroll follows the focus, and the prefetch length ahead of the motion.
std::optional<Collection::Range> Collection::findWindow(float prefetchLength) const {
    const std::size_t lines = sizes->getLineCount();
    if (lines == 0) {
        return std::nullopt;
    }
    const double start = getViewStart();
    const double end = start + getMainLength() - kSettleDistance;
    const double velocity = scroller->getVelocity();
    std::size_t first = sizes->findLine(velocity < 0.0 ? start - prefetchLength : start);
    std::size_t last = sizes->findLine(velocity >= 0.0 ? end + prefetchLength : end);
    first = first > 0 ? first - 1 : 0;
    last = std::min(last + 1, lines - 1);
    if (focusedIndex && *focusedIndex < sizes->size()) {
        const std::size_t line = layout->getLine(*focusedIndex);
        const double place = sizes->getLineOffset(line);
        if (place >= start - getMainLength() && place <= end + getMainLength()) {
            first = std::min(first, line > 0 ? line - 1 : 0);
            last = std::max(last, std::min(line + 1, lines - 1));
        }
    }
    return Range{.first = layout->getFirstIndex(first), .last = layout->getFirstIndex(last + 1) - 1};
}

// The current sticky item is the last one that starts before the start of the view.
std::optional<std::size_t> Collection::findSticky() const {
    if (stickyItems.empty()) {
        return std::nullopt;
    }
    const double start = getViewStart();
    const auto after = std::ranges::upper_bound(stickyItems, start, {}, [this](std::size_t index) { return sizes->getLineOffset(layout->getLine(index)); });
    if (after == stickyItems.begin()) {
        return std::nullopt;
    }
    const std::size_t item = *(after - 1);
    return sizes->getLineOffset(layout->getLine(item)) < start ? std::optional<std::size_t>(item) : std::nullopt;
}

std::optional<Collection::Hold> Collection::findHold() const {
    if (sizes->getLineCount() == 0 || renderedFrame == 0) {
        return std::nullopt;
    }
    if (stickToEnd && scroller->getOffset() >= scroller->getMaximum() - kSettleDistance) {
        return Hold{.index = 0, .distance = 0.0, .end = true};
    }
    // The hold prefers the focused item, then the first measured item in view, so an item that grows when it is measured for the first time grows away from what the player already saw.
    const double viewEnd = getViewStart() + getMainLength();
    std::size_t index = layout->getFirstIndex(sizes->findLine(getViewStart()));
    for (std::size_t line = sizes->findLine(getViewStart()); line < sizes->getLineCount() && sizes->getLineOffset(line) < viewEnd; ++line) {
        if (sizes->isMeasured(layout->getFirstIndex(line))) {
            index = layout->getFirstIndex(line);
            break;
        }
    }
    if (focusedIndex && *focusedIndex < sizes->size() && getItemRect(*focusedIndex).intersects(viewport)) {
        index = *focusedIndex;
    }
    return Hold{.index = index, .distance = getItemStart(index) - scroller->getOffset(), .end = false};
}

void Collection::holdView() {
    if (!hold) {
        hold = findHold();
    }
}

void Collection::restoreHold() {
    if (!hold) {
        return;
    }
    const Hold kept = *std::exchange(hold, std::nullopt);
    scroller->setMaximum(getContentLength() - getMainLength());
    if (kept.end) {
        scroller->shift(scroller->getMaximum() - scroller->getOffset());
    } else if (kept.index < sizes->size()) {
        scroller->shift(getItemStart(kept.index) - kept.distance - scroller->getOffset());
    }
}

// A released cell shows no item anymore, so a handle of its item reads as stale.
void Collection::release(const std::shared_ptr<CollectionCell>& cell) {
    cell->index = CollectionCell::kNoIndex;
    cell->waiting = false;
    cell->origin.reset();
    ++cell->generation;
    types->release(cell);
    released.push_back(cell);
}

// Only the cells that drew in the last frame of a collection that animates its changes move or fade, so changes out of view or while it is hidden apply at once.
bool Collection::isAnimating(const CollectionCell& cell) const noexcept {
    return animateChanges && renderedFrame > 0 && cell.root->drawnFrame == renderedFrame;
}

// Items inserted near the cells that show fade in once they are bound, and items far from them simply appear, so a whole new list costs nothing more.
void Collection::noteEntering(std::size_t first, std::size_t count) {
    if (!animateChanges || cells.empty() || renderedFrame == 0) {
        return;
    }
    const std::size_t low = cells.front()->index == CollectionCell::kNoIndex ? 0 : cells.front()->index;
    const std::size_t high = low + cells.size();
    for (std::size_t index = std::max(first, low); index < first + count && index <= high; ++index) {
        entering.emplace_back(source->getId(index));
    }
}

// Moved cells slide from where they drew before the change to their new place, new cells fade in, and the cells of removed items fade out and go back to their pools.
void Collection::animate() {
    const float decay = std::exp(-kSlideSpeed * delta);
    const double far = getMainLength() * 2.0;
    for (const std::shared_ptr<CollectionCell>& cell : cells) {
        if (cell->origin) {
            const math::Vec2 offset = *cell->origin - getItemRect(cell->index).getMin();
            cell->slide = std::fabs(offset.x) < far && std::fabs(offset.y) < far ? offset : math::Vec2{};
            cell->origin.reset();
        } else {
            cell->slide = cell->slide * decay;
            if (std::fabs(cell->slide.x) < kSettleDistance && std::fabs(cell->slide.y) < kSettleDistance) {
                cell->slide = {};
            }
        }
        cell->opacity = std::min(1.0F, cell->opacity + delta * kFadeSpeed);
    }
    // clang-format off
    std::erase_if(leaving, [this](const std::shared_ptr<CollectionCell>& cell) {
        cell->opacity -= delta * kFadeSpeed;
        if (cell->opacity > 0.0F) {
            return false;
        }
        cell->opacity = 1.0F;
        types->release(cell);
        released.push_back(cell);
        return true;
    });
    // clang-format on
    entering.clear();
}

// Cells a line beyond the window stay bound, so a player who turns back rebinds nothing, and so do the current sticky item and the item that waits for the focus.
void Collection::releaseOutside(const Range& window, std::optional<std::size_t> sticky) {
    const std::size_t lines = sizes->getLineCount();
    const std::size_t firstLine = layout->getLine(window.first);
    const std::size_t lastLine = layout->getLine(window.last);
    const std::size_t keepFirst = layout->getFirstIndex(firstLine > 0 ? firstLine - 1 : 0);
    const std::size_t keepLast = layout->getFirstIndex(std::min(lastLine + 2, lines)) - 1;
    // clang-format off
    std::erase_if(cells, [&](const std::shared_ptr<CollectionCell>& cell) {
        const bool kept = (cell->index >= keepFirst && cell->index <= keepLast) || cell->index == sticky || cell->index == pendingFocus;
        if (!kept) {
            release(cell);
        }
        return !kept;
    });
    // clang-format on
}

void Collection::bindCell(const std::shared_ptr<CollectionCell>& cell, std::size_t index, bool withBinders) {
    const std::uint16_t type = sizes->getType(index);
    const bool rebinding = cell->index == index;
    cell->type.bind(*cell, *source, index, selection != Selection::None && selected[index] != 0);
    if (!rebinding) {
        cell->slide = {};
        cell->origin.reset();
        cell->opacity = std::ranges::find(entering, cell->item) != entering.end() ? 0.0F : 1.0F;
    }
    cell->waiting = static_cast<bool>(binders[type]);
    if (cell->waiting && withBinders) {
        waiting.push_back(cell);
    }
}

// Items in the range without a cell take one of their type, from its pool or built from its template. While the GUIs draw, items of a type with a binder wait for the next preparation instead, since only it may run Lua.
void Collection::bindRange(std::size_t first, std::size_t last, bool withBinders) {
    auto position = std::ranges::lower_bound(cells, first, {}, [](const std::shared_ptr<CollectionCell>& cell) { return cell->index; });
    for (std::size_t index = first; index <= last; ++index) {
        if (position != cells.end() && (*position)->index == index) {
            const std::shared_ptr<CollectionCell>& cell = *position;
            if (cell->stale && (withBinders || !binders[sizes->getType(index)])) {
                bindCell(cell, index, withBinders);
            }
            ++position;
            continue;
        }
        const std::uint16_t type = sizes->getType(index);
        if (type == SizeCache::kNoType || (!withBinders && binders[type])) {
            continue;
        }
        std::shared_ptr<CollectionCell> cell = types->acquire(type);
        bindCell(cell, index, withBinders);
        position = cells.insert(position, std::move(cell)) + 1;
    }
}

// Binders run from a copy of the waiting cells, so a binder never runs while the collection walks its own cells, and a cell that moved on to another item meanwhile skips the binder of the old one.
void Collection::runBinders() {
    binderQueue.swap(waiting);
    for (const std::shared_ptr<CollectionCell>& cell : binderQueue) {
        if (cell->waiting && cell->index != CollectionCell::kNoIndex) {
            cell->waiting = false;
            binders[sizes->getType(cell->index)](*cell, cell->index);
        }
    }
    binderQueue.clear();
}

// Every bound cell measures every frame, as every container measures its children, and a length that changed moves the lines after it.
void Collection::measureCells(Context& context) {
    for (const std::shared_ptr<CollectionCell>& cell : cells) {
        const std::size_t index = cell->index;
        if (cell->waiting || sizes->isFixed(index)) {
            continue;
        }
        const float available = isVertical() ? layout->getSlot(index).crossLength : CommonProperties::kUnbounded;
        const math::Vec2 size = cell->root->measure(context, available);
        if (!isVertical()) {
            tallest = std::max(tallest, size.y);
        }
        sizes->measure(index, isVertical() ? size.y : size.x);
    }
}

void Collection::prepare(Context& context) {
    if (!source || renderedFrame == 0 || renderedFrame + 1 != context.getFrame()) {
        return;
    }
    rebuildTypes();
    resolveSelection();
    ensureArranged();
    scroller->update(context.getDeltaSeconds());

    // Pages may arrive at once, which changes the items before the window binds.
    const float ahead = prefetch * getMainLength();
    if (const std::optional<Range> window = findWindow(ahead)) {
        source->request(window->first, window->last - window->first + 1);
        ensureArranged();
    }
    const std::optional<Range> window = findWindow(ahead);
    if (!window) {
        return;
    }
    const std::optional<std::size_t> sticky = findSticky();
    releaseOutside(*window, sticky);
    {
        const BindingScope scope(binding);
        bindRange(window->first, window->last, true);
        if (sticky) {
            bindRange(*sticky, *sticky, true);
        }
        runBinders();
    }
    types->trim(static_cast<std::size_t>(poolSize));
}

// A vertical collection is as long as its content, within its size bounds, and a horizontal one is as tall as the tallest cell it has measured, which never shrinks while it lives.
math::Vec2 Collection::measureContent(Context& context, float availableWidth) {
    const bool bounded = availableWidth < CommonProperties::kUnbounded;
    auto children = getLayoutChildren();
    if (getCount() == 0 && !children.empty()) {
        return children.front()->measure(context, availableWidth);
    }
    if (isVertical()) {
        if (bounded && types) {
            updateGeometry(context, std::max(0.0F, availableWidth - padding.getHorizontal() - getLaneReserve(context)));
        }
        return {bounded ? availableWidth : 0.0F, static_cast<float>(getContentLength())};
    }
    const float height = (tallest > 0.0F ? tallest : context.getMetric(Theme::Metric::ListRowHeight)) + padding.getVertical() + getLaneReserve(context);
    return {static_cast<float>(getContentLength()), height};
}

void Collection::render(Context& context, const math::Rect& bounds) {
    renderedFrame = context.getFrame();
    delta = context.getDeltaSeconds();
    viewport = bounds;
    rightToLeft = context.isRightToLeft();
    context.getFocus().beginCollection(bounds, !isVertical(), rememberFocus ? rememberedTarget : 0);
    updateGeometry(context, (isVertical() ? bounds.width - padding.getHorizontal() : bounds.height - padding.getVertical()) - getLaneReserve(context));
    scroller->setMaximum(getContentLength() - getMainLength());
    if (getCount() == 0) {
        laneReserved = false;
        animate();
        drawEmpty(context);
        ImGui::PushClipRect(ImGuiConverter::toImVec2(bounds.getMin()), ImGuiConverter::toImVec2(bounds.getMax()), true);
        drawLeaving(context);
        ImGui::PopClipRect();
        report(context);
        noticeHiddenCells(context);
        return;
    }
    resolveSelection();
    restoreHold();
    takeInput(context);
    takePaging(context);
    resolveScroll();

    // Cells measured for the first time move the lines after them, so the view holds the item it shows while the window settles. Cells that a jump left behind go to the pools first, so the new items reuse them.
    for (int pass = 0; pass < 2; ++pass) {
        holdView();
        const std::optional<std::size_t> sticky = findSticky();
        releaseOutside(*findWindow(prefetch * getMainLength()), sticky);
        const std::optional<Range> window = findWindow(0.0F);
        bindRange(window->first, window->last, false);
        if (sticky) {
            bindRange(*sticky, *sticky, false);
        }
        if (pendingFocus && *pendingFocus < getCount()) {
            bindRange(*pendingFocus, *pendingFocus, false);
        }
        measureCells(context);
        ensureArranged();
        restoreHold();
    }
    types->trim(static_cast<std::size_t>(poolSize));
    settle();
    animate();

    // The bar shows once the cells left its lane, from the frame after the content grew longer than the view, and the lane goes the frame after the content fits again. The bar takes the pointer in its lane before the cells do, which draw only outside it.
    const std::optional<Range> window = findWindow(0.0F);
    const std::optional<std::size_t> sticky = findSticky();
    const bool overflowing = scrollbarShown && scroller->getMaximum() > 0.0;
    const bool barShown = overflowing && laneReserved;
    laneReserved = overflowing;
    const math::Rect cellArea = barShown ? Scrollbar::getContentBox(context, bounds, bounds, !isVertical()) : bounds;
    ImGui::PushClipRect(ImGuiConverter::toImVec2(bounds.getMin()), ImGuiConverter::toImVec2(bounds.getMax()), true);
    const std::optional<double> dragged = barShown ? scrollbar->interact(context, bounds, !isVertical(), rightToLeft && !isVertical(), scroller->getOffset(), scroller->getMaximum(), getMainLength()) : std::nullopt;
    ImGui::PushClipRect(ImGuiConverter::toImVec2(cellArea.getMin()), ImGuiConverter::toImVec2(cellArea.getMax()), true);
    drawCells(context, *window, sticky);
    drawLeaving(context);
    drawRefresh(context);
    ImGui::PopClipRect();
    if (barShown) {
        const bool touch = context.getInput().getLastDevice() == input::InputDevice::Touch;
        scrollbar->draw(context, touch ? std::clamp(1.0F - (idle - kScrollbarFadeDelay) / kScrollbarFade, 0.0F, 1.0F) : 1.0F);
    }
    ImGui::PopClipRect();
    if (dragged) {
        scroller->jumpTo(*dragged);
        pendingScroll.reset();
        idle = 0.0F;
    }
    context.blockPointer(bounds);
    report(context);
    noticeHiddenCells(context);
}

// The innermost collection under the pointer takes the wheel, a horizontal one the vertical wheel too, and a finger that drags along the axis past a small distance takes over from the control it pressed, unless that control edits text.
void Collection::takeInput(Context& context) {
    const ImGuiIO& io = ImGui::GetIO();
    const ImGuiContext& state = *GImGui;
    const bool hovered = ImGui::IsWindowHovered() && viewport.contains(math::Vec2{io.MousePos.x, io.MousePos.y});
    idle += delta;

    const float wheel = isVertical() ? -io.MouseWheel : -io.MouseWheel + (rightToLeft ? io.MouseWheelH : -io.MouseWheelH);
    if (hovered && wheel != 0.0F && scroller->getMaximum() > 0.0) {
        scroller->jumpTo(scroller->getOffset() + static_cast<double>(wheel * context.getMetric(Theme::Metric::ControlHeight)));
        pendingScroll.reset();
        idle = 0.0F;
    }

    if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
        pressed = hovered;
        dragging = false;
    }
    if (!ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
        if (dragging) {
            finishDrag(context);
        }
        pressed = false;
        return;
    }
    if (!pressed || io.MouseSource != ImGuiMouseSource_TouchScreen) {
        return;
    }
    if (!dragging) {
        const ImVec2 moved = ImGui::GetMouseDragDelta(ImGuiMouseButton_Left, 0.0F);
        const float along = std::fabs(isVertical() ? moved.y : moved.x);
        const float across = std::fabs(isVertical() ? moved.x : moved.y);
        if (along < kDragSlop || along <= across) {
            return;
        }
        if (state.ActiveId != 0 && io.WantTextInput) {
            pressed = false;
            return;
        }
        if (state.ActiveId != 0) {
            ImGui::ClearActiveID();
        }
        dragging = true;
        pendingScroll.reset();
    }
    const float step = isVertical() ? -io.MouseDelta.y : (rightToLeft ? io.MouseDelta.x : -io.MouseDelta.x);
    scroller->drag(step, delta);
    idle = 0.0F;
}

// A pull past the refresh distance at the start of a refreshable list asks the app to refresh it.
void Collection::finishDrag(Context& context) {
    const double pull = -scroller->getOverscroll();
    scroller->release();
    dragging = false;
    if (refreshable && isVertical() && !refreshing && pull >= context.getMetric(Theme::Metric::RefreshDistance)) {
        refreshing = true;
        context.emit(*this, "refresh");
    }
}

// Paging moves the focus to the first item of the next or previous view, or to the first or last item, and a wrap along the axis jumps to the other end.
void Collection::takePaging(Context& context) {
    const std::optional<FocusNavigator::Paging> paging = context.getFocus().takePaging();
    if (!paging) {
        return;
    }
    using Kind = FocusNavigator::Paging::Kind;
    const std::size_t last = getCount() - 1;
    std::size_t target = 0;
    ScrollAlign align = ScrollAlign::Start;
    if (paging->kind == Kind::Previous || paging->kind == Kind::Next) {
        const bool forward = paging->kind == Kind::Next;
        const double offset = scroller->getOffset();
        const double start = std::clamp(offset + (forward ? getMainLength() : -getMainLength()), 0.0, scroller->getMaximum());
        target = std::fabs(start - offset) < kSettleDistance ? (forward ? last : 0) : findFirstFrom(start);
    } else if (paging->kind == Kind::Last) {
        target = last;
        align = ScrollAlign::End;
    } else if (paging->kind == Kind::Wrap) {
        const bool forward = isVertical() ? paging->direction == FocusDirection::Down : (paging->direction == FocusDirection::Right) != rightToLeft;
        target = forward ? 0 : last;
        align = forward ? ScrollAlign::Start : ScrollAlign::End;
    }
    pendingFocus = target;
    requestScroll(target, {.align = align, .offset = 0.0F, .animated = true});
}

void Collection::requestScroll(std::size_t index, ScrollRequest request) {
    pendingScroll = PendingScroll{.index = index, .request = request, .distance = std::nullopt, .target = -1.0, .stableFrames = 0};
}

double Collection::findScrollTarget(const PendingScroll& pending) const {
    const double start = getItemStart(pending.index);
    if (pending.distance) {
        return start - *pending.distance;
    }
    const double length = sizes->getLineLength(layout->getLine(pending.index));
    const double view = getMainLength();
    const double inset = pending.request.offset;
    const double toStart = start - getPaddingStart() - inset;
    const double toEnd = start + length - view + getPaddingEnd() + inset;
    switch (pending.request.align) {
    case ScrollAlign::Start:
        return toStart;
    case ScrollAlign::End:
        return toEnd;
    case ScrollAlign::Center:
        return start + (length - view) * 0.5 - inset;
    case ScrollAlign::Nearest:
        break;
    }
    const double offset = scroller->getOffset();
    if (offset > toStart) {
        return toStart;
    }
    return offset < toEnd ? toEnd : offset;
}

// A scroll to an item aims again every frame, since items measured on the way move it, and ends once the item is measured and its place held still for a few frames.
void Collection::resolveScroll() {
    if (!pendingScroll) {
        return;
    }
    PendingScroll& pending = *pendingScroll;
    if (pending.index >= getCount()) {
        pendingScroll.reset();
        return;
    }
    const double target = std::clamp(findScrollTarget(pending), 0.0, scroller->getMaximum());
    const bool steady = std::fabs(target - pending.target) < kSettleDistance && sizes->isMeasured(pending.index);
    const bool arrived = std::fabs(scroller->getOffset() - target) < kSettleDistance;
    pending.stableFrames = steady ? pending.stableFrames + 1 : 0;
    pending.target = target;
    // A smooth scroll to a far item jumps to a view away from it first, so it neither takes long nor binds every item on the way.
    const double far = getMainLength() * 2.0;
    if (pending.request.animated && std::fabs(target - scroller->getOffset()) > far) {
        scroller->jumpTo(target + (target > scroller->getOffset() ? -getMainLength() : getMainLength()));
    }
    if (!pending.request.animated) {
        scroller->jumpTo(target);
    } else if (!arrived || !scroller->isResting()) {
        scroller->animateTo(target);
    }
    if (pending.stableFrames >= kStableFrames && arrived && scroller->isResting()) {
        pendingScroll.reset();
    }
}

// Once the player lets go and the content rests, it springs to the nearest place its snapping names.
void Collection::settle() {
    if (snap == Snap::None || pendingScroll || dragging || !scroller->isResting() || idle < kSnapDelay) {
        return;
    }
    const double point = std::clamp(findSnapPoint(), 0.0, scroller->getMaximum());
    if (std::fabs(point - scroller->getOffset()) > kSettleDistance) {
        scroller->animateTo(point);
    }
}

double Collection::findSnapPoint() const {
    const double offset = scroller->getOffset();
    const double view = getMainLength();
    if (snap == Snap::Page) {
        return view > 0.0 ? std::round(offset / view) * view : offset;
    }
    if (snap == Snap::Center) {
        const std::size_t line = sizes->findLine(getViewStart() + view * 0.5);
        return getPaddingStart() + sizes->getLineOffset(line) + (sizes->getLineLength(line) - view) * 0.5;
    }
    const std::size_t line = sizes->findLine(offset);
    const double before = sizes->getLineOffset(line);
    if (line + 1 >= sizes->getLineCount()) {
        return before;
    }
    const double after = sizes->getLineOffset(line + 1);
    return offset - before <= after - offset ? before : after;
}

// Cells draw in item order inside the clip of the view, and the current sticky item draws last at the start of the view, pushed out by the next one.
void Collection::drawCells(Context& context, const Range& window, std::optional<std::size_t> sticky) {
    const ImGuiID reveal = context.getFocus().getReveal();
    bool focusDrawn = false;
    for (const std::shared_ptr<CollectionCell>& cell : cells) {
        if (cell->index >= window.first && cell->index <= window.last && cell->index != sticky && !cell->waiting) {
            focusDrawn = drawCell(context, *cell, getItemRect(cell->index), reveal) || focusDrawn;
        }
    }
    if (sticky) {
        if (CollectionCell* cell = findCell(*sticky); cell != nullptr && !cell->waiting) {
            math::Rect rect = getItemRect(*sticky);
            const float length = isVertical() ? rect.height : rect.width;
            float pinned = 0.0F;
            const auto next = std::ranges::upper_bound(stickyItems, *sticky);
            if (next != stickyItems.end()) {
                const math::Rect following = getItemRect(*next);
                pinned = std::min(0.0F, isVertical() ? following.y - viewport.y - length : (rightToLeft ? viewport.getRight() - following.getRight() - length : following.x - viewport.x - length));
            }
            if (isVertical()) {
                rect.y = viewport.y + pinned;
            } else {
                rect.x = rightToLeft ? viewport.getRight() - length - pinned : viewport.x + pinned;
            }
            focusDrawn = drawCell(context, *cell, rect, reveal) || focusDrawn;
        }
    }
    // The item that waits for the focus takes it wherever it lies, and the next direction brings it into view.
    if (pendingFocus && (*pendingFocus < window.first || *pendingFocus > window.last) && *pendingFocus != sticky) {
        if (CollectionCell* cell = findCell(*pendingFocus); cell != nullptr && !cell->waiting) {
            focusDrawn = drawCell(context, *cell, getItemRect(cell->index), reveal) || focusDrawn;
        }
    }
    drawProxy(context, focusDrawn, reveal);
}

// A cell draws between the id of its item, so the ImGui state of its controls follows the item, with its events turned into events of the collection. An interactive cell is one focus target that paints its state behind its parts. Returns whether the cell holds the focus.
bool Collection::drawCell(Context& context, CollectionCell& cell, const math::Rect& layoutRect, ImGuiID reveal) {
    FocusNavigator& focus = context.getFocus();
    const math::Rect rect = layoutRect.translated(cell.slide);
    const std::size_t index = cell.index;
    if (cell.item.empty()) {
        ImGui::PushID(static_cast<int>(index));
    } else {
        ImGui::PushID(cell.item.data(), cell.item.data() + cell.item.size());
    }
    const std::size_t firstTarget = focus.getTargetCount();
    bool clicked = false;
    {
        const Context::EventRedirect redirect(context, *this, cell);
        focus.enterCell(*this, cell.item);
        if (cell.type.isInteractive() && !cell.item.empty()) {
            const float radius = context.getMetric(Theme::Metric::CellRadius);
            ImGui::SetNextItemAllowOverlap();
            const Widgets::Interaction state = Widgets::interact(context, rect, radius, "##cell");
            const bool chosen = selection != Selection::None && selected[index] != 0;
            if (context.getSurface(Theme::Surface::Cell) != nullptr) {
                Surfaces::draw(context, Theme::Surface::Cell, rect, context.getColor(Theme::Color::Raised), std::nullopt, radius);
            }
            if (chosen || state.held || state.hovered) {
                const Theme::Surface surface = chosen ? Theme::Surface::CellSelected : state.held ? Theme::Surface::CellPressed : Theme::Surface::CellHover;
                const Theme::Color color = chosen ? Theme::Color::Selection : state.held ? Theme::Color::Pressed : Theme::Color::Hover;
                Surfaces::draw(context, surface, rect, context.getColor(color), std::nullopt, radius);
            }
            clicked = state.clicked;
        }
        drawFaded(context, cell, rect);
        focus.leaveCell();
    }
    ImGui::PopID();

    if (clicked) {
        press(context, index);
    }
    if (reveal != 0 && focus.hasTargetSince(firstTarget, reveal)) {
        requestScroll(index, {.align = focusAlign, .offset = 0.0F, .animated = true});
    }
    if (pendingScroll && pendingScroll->index == index) {
        focus.markShownSince(firstTarget);
    }
    if (pendingFocus == index && focus.focusFirstSince(firstTarget)) {
        pendingFocus.reset();
    }
    const std::optional<ImGuiID> focused = focus.findFocusedSince(firstTarget);
    if (!focused) {
        return false;
    }
    focusedTarget = *focused;
    rememberedTarget = *focused;
    if (focusedIndex != index || focusedItem != cell.item) {
        focusedIndex = index;
        focusedItem = cell.item;
        context.emit(*this, "itemFocus", {{"item", cell.item}, {"index", index + 1}});
        if (selectionFollowsFocus && selection == Selection::Single && selected[index] == 0) {
            press(context, index);
        }
    }
    return true;
}

// A press selects the item as the selection mode says and reports it, again when the item already was selected.
void Collection::press(Context& context, std::size_t index) {
    core::Json values{{"item", source->getId(index)}, {"index", index + 1}};
    if (selection == Selection::Single) {
        for (std::size_t other = 0; other < selected.size(); ++other) {
            select(other, other == index);
        }
        values["selected"] = true;
    } else if (selection == Selection::Multiple) {
        select(index, selected[index] == 0);
        values["selected"] = selected[index] != 0;
    }
    context.emit(*this, "select", std::move(values));
}

void Collection::select(std::size_t index, bool value) {
    if ((selected[index] != 0) == value) {
        return;
    }
    selected[index] = value ? 1 : 0;
    if (CollectionCell* cell = findCell(index)) {
        cell->type.bindSelection(*cell, value);
    }
}

// The focused item whose cell does not draw keeps a target at its place, so the focus stays on it until the player asks to see it again. Focus that went elsewhere leaves the collection.
void Collection::drawProxy(Context& context, bool focusDrawn, ImGuiID reveal) {
    if (focusDrawn) {
        return;
    }
    if (focusedTarget == 0 || GImGui->NavId != focusedTarget || !focusedIndex || *focusedIndex >= getCount() || source->getId(*focusedIndex) != focusedItem) {
        focusedTarget = 0;
        focusedIndex.reset();
        focusedItem.clear();
        return;
    }
    FocusNavigator& focus = context.getFocus();
    focus.enterCell(*this, focusedItem);
    focus.addProxyTarget(focusedTarget, getItemRect(*focusedIndex));
    focus.leaveCell();
    if (reveal == focusedTarget) {
        requestScroll(*focusedIndex, {.align = focusAlign, .offset = 0.0F, .animated = true});
    }
}

// A cell draws through a reshape that fades it while its opacity is below one, the way a node fades with its transform.
void Collection::drawFaded(Context& context, CollectionCell& cell, const math::Rect& rect) {
    cell.placed = rect;
    if (cell.opacity >= 1.0F) {
        cell.root->draw(context, rect);
        return;
    }
    Transform faded;
    faded.opacity = cell.opacity;
    const int firstVertex = ImGui::GetWindowDrawList()->VtxBuffer.Size;
    context.pushReshape(faded, rect.getCenter());
    cell.root->draw(context, rect);
    context.popReshape();
    Component::reshape(firstVertex, faded, rect.getCenter());
}

// The cells of removed items draw where they were while they fade out, disabled, so they take neither input nor the focus.
void Collection::drawLeaving(Context& context) {
    for (const std::shared_ptr<CollectionCell>& cell : leaving) {
        if (cell->item.empty()) {
            ImGui::PushID(cell.get());
        } else {
            ImGui::PushID(cell->item.data(), cell->item.data() + cell->item.size());
        }
        const Context::EventRedirect redirect(context, *this, *cell);
        ImGui::BeginDisabled();
        drawFaded(context, *cell, cell->placed);
        ImGui::EndDisabled();
        ImGui::PopID();
    }
}

void Collection::drawEmpty(Context& context) {
    auto children = getLayoutChildren();
    if (!children.empty()) {
        children.front()->draw(context, viewport);
    }
}

// The busy indicator grows with a pull at the start and stays there while the collection refreshes.
void Collection::drawRefresh(Context& context) {
    const double pull = dragging ? -scroller->getOverscroll() : 0.0;
    if (!refreshable || !isVertical() || (!refreshing && pull <= 0.0)) {
        return;
    }
    const float distance = context.getMetric(Theme::Metric::RefreshDistance);
    const float reached = refreshing ? 1.0F : std::min(1.0F, static_cast<float>(pull) / std::max(1.0F, distance));
    const float radius = context.getMetric(Theme::Metric::ControlHeight) * 0.3F;
    const math::Vec2 center{viewport.getCenter().x, viewport.y + distance * 0.5F * reached};
    ImGui::GetWindowDrawList()->AddCircleFilled(ImGuiConverter::toImVec2(center), radius * 1.6F, ImGuiConverter::toImU32(context.getColor(Theme::Color::Raised)));
    const math::Color accent = context.getColor(Theme::Color::Accent);
    Widgets::spinner(context, center, radius, accent.withAlpha(accent.a * reached));
}

// The visible range, the ends of the items and the end of a scroll are reported when they change, at most once per frame.
void Collection::report(Context& context) {
    const std::size_t count = getCount();
    std::optional<std::pair<std::size_t, std::size_t>> range;
    if (count > 0 && sizes->getLineCount() > 0) {
        const double start = getViewStart();
        const std::size_t first = layout->getFirstIndex(sizes->findLine(start));
        const std::size_t last = layout->getFirstIndex(sizes->findLine(start + getMainLength() - kSettleDistance) + 1) - 1;
        range = std::pair{first, last};
    }
    if (range && range != visibleRange) {
        context.emit(*this, "visibleChange", {{"first", range->first + 1}, {"last", range->second + 1}});
    }
    visibleRange = range;
    if (range && range->second + static_cast<std::size_t>(endThreshold) + 1 >= count && endReported != count) {
        endReported = count;
        context.emit(*this, "endReached", {{"count", count}});
    }
    if (range && range->first <= static_cast<std::size_t>(endThreshold) && startReported != count) {
        startReported = count;
        context.emit(*this, "startReached", {{"count", count}});
    }

    const bool scrolling = dragging || !scroller->isResting();
    if (moving && !scrolling && range) {
        const std::size_t first = std::min(findFirstFrom(getViewStart()), count - 1);
        context.emit(*this, "scrollEnd", {{"offset", core::JsonNumber::fromFloat(static_cast<float>(scroller->getOffset()))}, {"item", source->getId(first)}});
    }
    moving = scrolling;
}

// Cells that drew in the last frame and not in this one, such as released cells, hear that they stopped drawing, with their events turned into events of the collection.
void Collection::noticeHiddenCells(Context& context) {
    for (const std::shared_ptr<CollectionCell>& cell : released) {
        const Context::EventRedirect redirect(context, *this, *cell);
        cell->root->noticeStoppedDrawing(context);
    }
    released.clear();
    for (const std::shared_ptr<CollectionCell>& cell : cells) {
        if (cell->root->drawnFrame + 1 == context.getFrame()) {
            const Context::EventRedirect redirect(context, *this, *cell);
            cell->root->noticeStoppedDrawing(context);
        }
    }
}

void Collection::drawingStopped(Context& context) {
    for (const std::shared_ptr<CollectionCell>& cell : leaving) {
        cell->opacity = 1.0F;
        types->release(cell);
        released.push_back(cell);
    }
    leaving.clear();
    for (const std::shared_ptr<CollectionCell>& cell : released) {
        const Context::EventRedirect redirect(context, *this, *cell);
        cell->root->noticeStoppedDrawing(context);
    }
    released.clear();
    for (const std::shared_ptr<CollectionCell>& cell : cells) {
        const Context::EventRedirect redirect(context, *this, *cell);
        cell->root->noticeStoppedDrawing(context);
    }
}

Event Collection::toCellEvent(CollectionCell& cell, const Component& component, std::string name, core::Json value) {
    if (!value.is_object()) {
        value = core::Json::object();
    }
    const bool notice = name == "focus" || name == "blur" || name == "cancel";
    if (!notice && cell.index != CollectionCell::kNoIndex) {
        cell.type.writeBack(cell, component, *source);
    }
    value["part"] = component.getId();
    value["cell"] = {{"item", cell.item}, {"index", cell.index == CollectionCell::kNoIndex ? core::Json() : core::Json(cell.index + 1)}, {"type", cell.getType()}};
    return {.id = getId(), .name = std::move(name), .value = std::move(value)};
}

void Collection::command(Context& context, std::string_view name, const core::Json& arguments) {
    if (name != "focus") {
        Component::command(context, name, arguments);
        return;
    }
    if (!arguments.is_null() && !(arguments.is_object() && arguments.empty())) {
        throw std::invalid_argument("The \"focus\" command takes no arguments.");
    }
    if (getCount() > 0) {
        focusItem(std::min(focusedIndex.value_or(visibleRange ? visibleRange->first : 0), getCount() - 1));
    }
}

// A layout of the app stays until another one replaces it, and none goes back to the layout the `layout` property chooses.
void Collection::setLayout(std::unique_ptr<CollectionLayout> value) {
    builtLayout.reset();
    customLayout = static_cast<bool>(value);
    layout = value ? std::move(value) : std::make_unique<LinearLayout>();
    layoutDirty = true;
}

void Collection::setBinder(std::string_view type, Binder binder) {
    const std::optional<std::uint16_t> found = types->find(type);
    if (!found) {
        throw std::invalid_argument("The collection \"" + getId() + "\" has no type named \"" + std::string(type) + "\".");
    }
    binders[*found] = std::move(binder);
    for (const std::shared_ptr<CollectionCell>& cell : cells) {
        cell->stale = cell->stale || &cell->type == &types->get(*found);
    }
}

std::optional<std::size_t> Collection::findItem(std::string_view id) const {
    for (std::size_t index = 0; source && index < source->getCount(); ++index) {
        if (source->getId(index) == id) {
            return index;
        }
    }
    return std::nullopt;
}

void Collection::scrollTo(std::size_t index, const ScrollRequest& request) {
    hold.reset();
    requestScroll(index, request);
}

// An offset the app asks for applies to the items as they are now, so the lines of a change it made meanwhile arrange first, and the view no longer holds what it showed.
void Collection::refreshRange() {
    hold.reset();
    if (renderedFrame > 0) {
        ensureArranged();
        scroller->setMaximum(getContentLength() - getMainLength());
    }
}

void Collection::scrollBy(double distance, bool animated) {
    refreshRange();
    pendingScroll.reset();
    const double target = std::clamp((scroller->isAnimating() ? scroller->getTarget() : scroller->getOffset()) + distance, 0.0, scroller->getMaximum());
    if (animated) {
        scroller->animateTo(target);
    } else {
        scroller->jumpTo(target);
    }
}

double Collection::getScrollOffset() const noexcept {
    return scroller->getOffset();
}

void Collection::setScrollOffset(double value) {
    refreshRange();
    pendingScroll.reset();
    scroller->jumpTo(value);
}

void Collection::focusItem(std::size_t index) {
    pendingFocus = index;
    requestScroll(index, {.align = focusAlign, .offset = 0.0F, .animated = true});
}

std::vector<std::size_t> Collection::getSelected() const {
    std::vector<std::size_t> indices;
    for (std::size_t index = 0; index < selected.size(); ++index) {
        if (selected[index] != 0) {
            indices.push_back(index);
        }
    }
    return indices;
}

Collection::State Collection::saveState() const {
    State saved;
    if (visibleRange && source) {
        const std::size_t first = visibleRange->first;
        saved.item = source->getId(first);
        saved.distance = static_cast<float>(getItemStart(first) - scroller->getOffset());
    }
    saved.focused = focusedItem;
    return saved;
}

// The saved item goes back to its saved distance from the start of the view and the saved focus returns, for the items that still exist.
void Collection::restoreState(const State& value) {
    if (const std::optional<std::size_t> index = findItem(value.item)) {
        pendingScroll = PendingScroll{.index = *index, .request = {.align = ScrollAlign::Start, .offset = 0.0F, .animated = false}, .distance = value.distance, .target = -1.0, .stableFrames = 0};
    }
    if (const std::optional<std::size_t> focused = value.focused.empty() ? std::nullopt : findItem(value.focused)) {
        pendingFocus = focused;
    }
}

CollectionCell* Collection::findCell(std::size_t index) const noexcept {
    const auto found = std::ranges::lower_bound(cells, index, {}, [](const std::shared_ptr<CollectionCell>& cell) { return cell->index; });
    return found != cells.end() && (*found)->index == index ? found->get() : nullptr;
}

} // namespace haylen::ui
