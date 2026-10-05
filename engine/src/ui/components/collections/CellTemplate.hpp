#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <vector>

#include "haylen/core/Json.hpp"
#include "haylen/ui/Transform.hpp"

namespace haylen::ui {

class CollectionCell;
class CollectionSource;
class Component;
class ComponentRegistry;

// One item type of a collection: its template compiled into a flat list of nodes with their parts and bindings, from which it builds cells, binds them to items and resets them before they show another item.
class CellTemplate final {
  public:
    // Where a binding takes its value: a field of the item, the index of the item from 1 or whether it is selected.
    enum class Source : std::uint8_t {
        Field,
        Index,
        Selected,
    };

    struct Binding {
        std::size_t node = 0;
        std::string property;
        std::string field;
        Source source = Source::Field;
    };

    // A node of the template in depth-first order, whose subtree ends before the node `end`.
    struct Node {
        std::string kind;
        core::Json properties;
        std::string part;
        std::size_t parent = kNoParent;
        std::size_t slot = 0;
        std::size_t end = 0;
        bool focusable = false;
    };

    static constexpr std::size_t kNoParent = std::numeric_limits<std::size_t>::max();
    static constexpr int kMaxSpan = 64;

    // Throws `std::invalid_argument` with a message that names the type for every problem of the definition and its template.
    CellTemplate(const ComponentRegistry& components, std::string typeName, const core::Json& definition);

    [[nodiscard]] std::shared_ptr<CollectionCell> build() const;

    // Gives a cell the item at an index: what the previous item left goes back to the template, and then the bindings apply.
    void bind(CollectionCell& cell, const CollectionSource& source, std::size_t index, bool selected) const;

    // Applies the bindings of `$index` again after the item moved, or of `$selected` after its selection changed.
    void bindIndex(CollectionCell& cell, std::size_t index) const;
    void bindSelection(CollectionCell& cell, bool selected) const;

    // Stores the values the player changed in a part of a bound cell into the fields its bindings read, so the item shows them when it is bound again, and marks the part to go back to its template before the cell shows another item.
    void writeBack(CollectionCell& cell, const Component& component, CollectionSource& source) const;

    void set(CollectionCell& cell, std::size_t node, const core::Json& properties) const;
    [[nodiscard]] const std::shared_ptr<Transform>& getTransform(CollectionCell& cell, std::size_t node) const;

    [[nodiscard]] std::optional<std::size_t> findPart(std::string_view part) const;
    [[nodiscard]] const std::string& getName() const noexcept {
        return name;
    }
    [[nodiscard]] const std::vector<Node>& getNodes() const noexcept {
        return nodes;
    }
    [[nodiscard]] float getEstimatedSize() const noexcept {
        return estimatedSize;
    }

    // The lanes a cell spans in a grid, where zero spans the whole line.
    [[nodiscard]] std::uint16_t getSpan() const noexcept {
        return span;
    }
    [[nodiscard]] bool isSticky() const noexcept {
        return sticky;
    }
    [[nodiscard]] bool isInteractive() const noexcept {
        return interactive;
    }
    [[nodiscard]] bool bindsIndex() const noexcept {
        return indexBound;
    }
    [[nodiscard]] bool bindsSelection() const noexcept {
        return selectionBound;
    }

  private:
    static constexpr std::array<std::string_view, 5> kKeys{"template", "estimatedSize", "span", "sticky", "interactive"};
    static constexpr std::array<std::string_view, 5> kReservedProperties{"kind", "id", "children", "part", "bind"};
    static constexpr std::array<std::string_view, 2> kRejectedKinds{"scroll", "collection"};
    static constexpr float kMaxEstimatedSize = 100000.0F;

    void readDefinition(const core::Json& definition);
    void flatten(const core::Json& node, std::size_t parent, std::size_t slot, std::size_t depth, std::set<std::string, std::less<>>& parts);
    void readBindings(const core::Json& bind, std::size_t node);

    [[nodiscard]] std::unique_ptr<Component> create(std::size_t node, std::vector<Component*>& created) const;

    // Builds the subtree of a node again from the template, for a node whose state the template cannot restore otherwise.
    void rebuild(CollectionCell& cell, std::size_t node) const;
    void reset(CollectionCell& cell) const;

    // Applies the bindings of one source, or of every source with `std::nullopt`.
    void applyBindings(CollectionCell& cell, const CollectionSource* source, std::size_t index, bool selected, std::optional<Source> only) const;
    [[nodiscard]] std::string describeNode(std::size_t node) const;

    const ComponentRegistry& registry;
    std::string name;
    std::vector<Node> nodes;
    std::vector<Binding> bindings;
    float estimatedSize = 0.0F;
    std::uint16_t span = 1;
    bool sticky = false;
    bool interactive = false;
    bool indexBound = false;
    bool selectionBound = false;
};

} // namespace haylen::ui
