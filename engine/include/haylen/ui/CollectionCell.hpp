#pragma once

#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "haylen/core/Json.hpp"
#include "haylen/debug/ObjectCounter.hpp"
#include "haylen/debug/TrackedObject.hpp"
#include "haylen/math/Rect.hpp"
#include "haylen/math/Vec2.hpp"
#include "haylen/ui/Component.hpp"
#include "haylen/ui/Transform.hpp"

namespace haylen::ui {

class CellTemplate;

// One cell of a collection: a tree of components built from the template of a type, which shows one item at a time and moves on to another item when the collection reuses it. Its parts are the nodes of the template that name themselves with `part`. Cells count themselves as `UiCell` in the debug statistics.
class CollectionCell final : public std::enable_shared_from_this<CollectionCell> {
  public:
    // The index of a cell that shows no item, such as a cell waiting in the pool of its type.
    static constexpr std::size_t kNoIndex = std::numeric_limits<std::size_t>::max();

    explicit CollectionCell(const CellTemplate& owner);

    CollectionCell(const CollectionCell&) = delete;
    CollectionCell& operator=(const CollectionCell&) = delete;

    [[nodiscard]] Component& getRoot() const noexcept {
        return *root;
    }

    // Returns the component of a part, or null when the template has no part with that name.
    [[nodiscard]] Component* findPart(std::string_view part) const;

    // Changes properties of a part, checked like `Gui::set` before anything changes. The part takes the values of its template again before the cell shows another item. Throws for an unknown part or an invalid value.
    void set(std::string_view part, const core::Json& properties);

    // Returns the transform of a part, which a new one replaces before the cell shows another item, so a tween on it stops with the item.
    [[nodiscard]] const std::shared_ptr<Transform>& getTransform(std::string_view part);

    // The id of the item the cell shows, empty for an item that did not load yet.
    [[nodiscard]] const std::string& getItem() const noexcept {
        return item;
    }
    [[nodiscard]] std::size_t getIndex() const noexcept {
        return index;
    }
    [[nodiscard]] std::string_view getType() const noexcept;

    // Counts the items the cell has shown, so a handle can tell that the cell moved on to another item.
    [[nodiscard]] std::uint64_t getGeneration() const noexcept {
        return generation;
    }

  private:
    friend class CellTemplate;
    friend class CellTypes;
    friend class Collection;

    static debug::ObjectCounter& counter;

    [[nodiscard]] std::size_t requirePart(std::string_view part) const;

    const CellTemplate& type;
    std::unique_ptr<Component> root;

    // Every node of the template in its order, with what changed it since the cell was bound: the player, `set` and its keys, or a transform handed out.
    std::vector<Component*> nodes;
    std::vector<std::uint8_t> touched;
    std::vector<std::vector<std::string>> changedKeys;
    std::vector<std::uint8_t> transformed;

    std::string item;
    std::size_t index = kNoIndex;
    std::uint64_t generation = 0;

    // A cell waits for the binder of its type, and binds again after its item changed in place.
    bool waiting = false;
    bool stale = false;

    // Where the cell drew last, where it drew before a change of the items, how far it still lies from its place while it slides there, and how much it shows while it fades in or out.
    math::Rect placed;
    std::optional<math::Vec2> origin;
    math::Vec2 slide;
    float opacity = 1.0F;
    debug::TrackedObject tracked{counter};
};

} // namespace haylen::ui
