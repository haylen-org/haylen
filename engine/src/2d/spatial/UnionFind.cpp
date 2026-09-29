#include "haylen/2d/spatial/UnionFind.hpp"

#include <numeric>
#include <stdexcept>
#include <string>
#include <utility>

namespace haylen::spatial2d {

UnionFind::UnionFind(std::size_t count) {
    reset(count);
}

void UnionFind::reset(std::size_t count) {
    parents.resize(count);
    std::iota(parents.begin(), parents.end(), std::size_t{0});
    sizes.assign(count, 1);
    sets = count;
}

void UnionFind::requireElement(std::size_t element) const {
    if (element >= parents.size()) {
        throw std::out_of_range("Element " + std::to_string(element) + " is outside the union-find of " + std::to_string(parents.size()) + " elements.");
    }
}

std::size_t UnionFind::add() {
    parents.push_back(parents.size());
    sizes.push_back(1);
    ++sets;
    return parents.size() - 1;
}

std::size_t UnionFind::find(std::size_t element) {
    requireElement(element);
    while (parents[element] != element) {
        parents[element] = parents[parents[element]];
        element = parents[element];
    }
    return element;
}

bool UnionFind::unite(std::size_t first, std::size_t second) {
    std::size_t larger = find(first);
    std::size_t smaller = find(second);
    if (larger == smaller) {
        return false;
    }
    if (sizes[larger] < sizes[smaller]) {
        std::swap(larger, smaller);
    }
    parents[smaller] = larger;
    sizes[larger] += sizes[smaller];
    --sets;
    return true;
}

bool UnionFind::isConnected(std::size_t first, std::size_t second) {
    return find(first) == find(second);
}

std::size_t UnionFind::getSetSize(std::size_t element) {
    return sizes[find(element)];
}

} // namespace haylen::spatial2d
