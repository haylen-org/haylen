#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string_view>
#include <vector>

#include "haylen/core/Json.hpp"
#include "ui/components/collections/CellTemplate.hpp"

namespace haylen::ui {

class CollectionCell;
class ComponentRegistry;

// The item types of one collection with their compiled templates, and a pool of detached cells for each type, so a cell that leaves the view waits for the next item of its type instead of being destroyed.
class CellTypes final {
  public:
    // Throws `std::invalid_argument` for a definition that is not an object of types or for any problem of a type.
    CellTypes(const ComponentRegistry& registry, const core::Json& definitions);

    [[nodiscard]] std::size_t size() const noexcept {
        return templates.size();
    }
    [[nodiscard]] const CellTemplate& get(std::uint16_t type) const noexcept {
        return *templates[type];
    }
    [[nodiscard]] std::optional<std::uint16_t> find(std::string_view name) const noexcept;

    // Hands out a detached cell of the type, or builds a new one when its pool is empty.
    [[nodiscard]] std::shared_ptr<CollectionCell> acquire(std::uint16_t type);

    // Keeps a detached cell in the pool of its type, where the next item of the type finds it.
    void release(const std::shared_ptr<CollectionCell>& cell);

    // Lets go of the cells beyond `limit` in every pool, once the cells a pass released found their new items.
    void trim(std::size_t limit);

  private:
    static constexpr std::size_t kMaxTypes = 4096;

    std::vector<std::unique_ptr<CellTemplate>> templates;
    std::vector<std::vector<std::shared_ptr<CollectionCell>>> pools;
};

} // namespace haylen::ui
