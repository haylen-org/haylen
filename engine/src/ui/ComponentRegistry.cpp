#include "haylen/ui/ComponentRegistry.hpp"

#include <stdexcept>

namespace haylen::ui {

void ComponentRegistry::add(std::string kind, Factory factory) {
    if (kind.empty() || !factory) {
        throw std::invalid_argument("A component kind needs a name and a factory.");
    }
    if (factories.contains(kind)) {
        throw std::invalid_argument("The component kind \"" + kind + "\" is already registered.");
    }
    factories.emplace(std::move(kind), std::move(factory));
}

std::unique_ptr<Component> ComponentRegistry::create(std::string_view kind) const {
    const auto factory = factories.find(kind);
    if (factory == factories.end()) {
        throw std::invalid_argument("There is no UI component kind named \"" + std::string(kind) + "\".");
    }
    return factory->second();
}

bool ComponentRegistry::contains(std::string_view kind) const {
    return factories.contains(kind);
}

std::vector<std::string> ComponentRegistry::getKinds() const {
    std::vector<std::string> names;
    for (const auto& [kind, factory] : factories) {
        names.push_back(kind);
    }
    return names;
}

} // namespace haylen::ui
