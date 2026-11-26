#include "haylen/3d/nodes/ComponentStore3D.hpp"

#include <stdexcept>
#include <string>
#include <utility>

#include "haylen/3d/nodes/NodeTree3D.hpp"

namespace haylen::nodes3d {

void ComponentStore3D::add(const Node3D& node, const std::shared_ptr<const ComponentType3D>& type, const core::Json& options) {
    (void)tree.check(node);
    if (!type) {
        throw std::invalid_argument("A component needs a type.");
    }

    Pool* pool = findPool(*type);
    if (pool == nullptr) {
        pools.push_back({.type = type, .components = type->createPool(tree)});
        pool = &pools.back();
    }

    if (pool->places.contains(node.getKey())) {
        throw std::invalid_argument("The node \"" + node.getPath() + "\" holds a \"" + std::string(type->getName()) + "\" component already.");
    }

    pool->components->add(node, options);
    pool->places.emplace(node.getKey(), pool->nodes.size());
    pool->nodes.push_back(node);
    pool->active.push_back(node.isWorldEnabled() ? 1 : 0);
}

void ComponentStore3D::remove(const Node3D& node, const ComponentType3D& type) {
    (void)tree.check(node);
    Pool* pool = findPool(type);
    if (pool == nullptr) {
        return;
    }

    if (const auto found = pool->places.find(node.getKey()); found != pool->places.end()) {
        removeAt(*pool, found->second);
    }
}

bool ComponentStore3D::has(const Node3D& node, const ComponentType3D& type) const {
    (void)tree.check(node);
    const Pool* pool = findPool(type);
    return pool != nullptr && pool->places.contains(node.getKey());
}

std::vector<std::shared_ptr<const ComponentType3D>> ComponentStore3D::getTypes(const Node3D& node) const {
    (void)tree.check(node);
    std::vector<std::shared_ptr<const ComponentType3D>> types;
    for (const Pool& pool : pools) {
        if (pool.places.contains(node.getKey())) {
            types.push_back(pool.type);
        }
    }

    return types;
}

core::Json ComponentStore3D::get(const Node3D& node, const ComponentType3D& type, std::string_view property) const {
    const Pool* pool = findPool(type);
    if (pool == nullptr) {
        (void)tree.check(node);
        throw std::invalid_argument("The node \"" + node.getPath() + "\" holds no \"" + std::string(type.getName()) + "\" component.");
    }

    return pool->components->get(place(*pool, node), property);
}

void ComponentStore3D::set(const Node3D& node, const ComponentType3D& type, std::string_view property, const core::Json& value) {
    Pool* pool = findPool(type);
    if (pool == nullptr) {
        (void)tree.check(node);
        throw std::invalid_argument("The node \"" + node.getPath() + "\" holds no \"" + std::string(type.getName()) + "\" component.");
    }

    pool->components->set(place(*pool, node), property, value);
}

core::Json ComponentStore3D::save(const Node3D& node) const {
    (void)tree.check(node);
    core::Json saved = core::Json::object();
    for (const Pool& pool : pools) {
        if (const auto found = pool.places.find(node.getKey()); found != pool.places.end()) {
            saved[std::string(pool.type->getName())] = pool.components->save(found->second);
        }
    }

    return saved;
}

core::Json ComponentStore3D::save(const Node3D& node, const ComponentType3D& type) const {
    const Pool* pool = findPool(type);
    if (pool == nullptr) {
        (void)tree.check(node);
        throw std::invalid_argument("The node \"" + node.getPath() + "\" holds no \"" + std::string(type.getName()) + "\" component.");
    }

    return pool->components->save(place(*pool, node));
}

void ComponentStore3D::load(const Node3D& node, const core::Json& components, const ComponentRegistry3D& registry) {
    (void)tree.check(node);
    if (!components.is_object()) {
        throw std::invalid_argument("The components of a node are an object of the options of each component by the name of its type.");
    }

    std::vector<std::shared_ptr<const ComponentType3D>> types;
    for (const auto& [name, options] : components.items()) {
        std::shared_ptr<const ComponentType3D> type = registry.find(name);
        if (!type) {
            throw std::invalid_argument("No component type is named \"" + name + "\". Register it in the component types of the 3D nodes plugin.");
        }

        types.push_back(std::move(type));
    }

    for (const std::shared_ptr<const ComponentType3D>& type : types) {
        add(node, type, components.at(std::string(type->getName())));
    }
}

// The pools only grow, so a phase walks the pools the store had when it began, and a pool reads its arrays without a copy.
void ComponentStore3D::run(ComponentType3D::Phase phase, float deltaSeconds) {
    const std::size_t count = pools.size();
    for (std::size_t index = 0; index < count; ++index) {
        Pool& pool = pools[index];
        if (pool.nodes.empty() || !pool.type->runsIn(phase)) {
            continue;
        }

        pool.components->run(phase, {.nodes = pool.nodes, .active = pool.active, .deltaSeconds = deltaSeconds});
    }
}

std::size_t ComponentStore3D::size() const noexcept {
    std::size_t count = 0;
    for (const Pool& pool : pools) {
        count += pool.nodes.size();
    }

    return count;
}

ComponentStore3D::Pool* ComponentStore3D::findPool(const ComponentType3D& type) noexcept {
    for (Pool& pool : pools) {
        if (pool.type.get() == &type) {
            return &pool;
        }
    }

    return nullptr;
}

const ComponentStore3D::Pool* ComponentStore3D::findPool(const ComponentType3D& type) const noexcept {
    for (const Pool& pool : pools) {
        if (pool.type.get() == &type) {
            return &pool;
        }
    }

    return nullptr;
}

std::size_t ComponentStore3D::place(const Pool& pool, const Node3D& node) const {
    (void)tree.check(node);
    const auto found = pool.places.find(node.getKey());
    if (found == pool.places.end()) {
        throw std::invalid_argument("The node \"" + node.getPath() + "\" holds no \"" + std::string(pool.type->getName()) + "\" component.");
    }

    return found->second;
}

// The last component takes the freed place in the pool and in the lists of the store alike.
void ComponentStore3D::removeAt(Pool& pool, std::size_t index) {
    const std::size_t last = pool.nodes.size() - 1;
    pool.components->remove(index);
    pool.places.erase(pool.nodes[index].getKey());
    if (index != last) {
        pool.nodes[index] = pool.nodes[last];
        pool.active[index] = pool.active[last];
        pool.places[pool.nodes[index].getKey()] = index;
    }

    pool.nodes.pop_back();
    pool.active.pop_back();
}

void ComponentStore3D::removeNode(const Node3D& node) {
    for (Pool& pool : pools) {
        if (const auto found = pool.places.find(node.getKey()); found != pool.places.end()) {
            removeAt(pool, found->second);
        }
    }
}

void ComponentStore3D::copyNode(const Node3D& source, const Node3D& copy) {
    const std::size_t count = pools.size();
    for (std::size_t index = 0; index < count; ++index) {
        Pool& pool = pools[index];
        if (const auto found = pool.places.find(source.getKey()); found != pool.places.end()) {
            const core::Json options = pool.components->save(found->second);
            add(copy, pool.type, options);
        }
    }
}

void ComponentStore3D::setActive(const Node3D& node, bool value) {
    for (Pool& pool : pools) {
        if (const auto found = pool.places.find(node.getKey()); found != pool.places.end()) {
            pool.active[found->second] = value ? 1 : 0;
        }
    }
}

} // namespace haylen::nodes3d
