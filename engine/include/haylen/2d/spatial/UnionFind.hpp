#pragma once

#include <cstddef>
#include <vector>

namespace haylen::spatial2d {

// Disjoint sets of the elements 0 to `size - 1` with union by size and path halving, which keeps every operation nearly constant time.
class UnionFind final {
  public:
    explicit UnionFind(std::size_t count = 0);

    // Starts over with `count` elements, each in a set of its own.
    void reset(std::size_t count);

    // Adds an element in a set of its own and returns it.
    std::size_t add();

    // Returns the representative element of the set that holds the element.
    [[nodiscard]] std::size_t find(std::size_t element);

    // Merges the sets of both elements and returns `false` when they already shared one.
    bool unite(std::size_t first, std::size_t second);
    [[nodiscard]] bool isConnected(std::size_t first, std::size_t second);
    [[nodiscard]] std::size_t getSetSize(std::size_t element);

    [[nodiscard]] std::size_t getSetCount() const noexcept {
        return sets;
    }
    [[nodiscard]] std::size_t size() const noexcept {
        return parents.size();
    }

  private:
    void requireElement(std::size_t element) const;

    std::vector<std::size_t> parents;
    std::vector<std::size_t> sizes;
    std::size_t sets = 0;
};

} // namespace haylen::spatial2d
