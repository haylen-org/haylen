#include "haylen/3d/physics/BackendRegistry.hpp"

#include <format>
#include <stdexcept>

namespace haylen::physics3d {

void BackendRegistry::add(std::unique_ptr<Backend> backend) {
    const std::string name(backend->getName());
    if (backends.contains(name)) {
        throw std::invalid_argument(std::format("A 3D physics backend named \"{}\" is registered already.", name));
    }

    Backend& added = *backends.emplace(name, std::move(backend)).first->second;
    if (selected == nullptr) {
        selected = &added;
    }
}

void BackendRegistry::select(std::string_view name) {
    const auto found = backends.find(name);
    if (found == backends.end()) {
        std::string names;
        for (const auto& [known, backend] : backends) {
            names += names.empty() ? std::format("\"{}\"", known) : std::format(", \"{}\"", known);
        }

        throw std::invalid_argument(std::format("No 3D physics backend is named \"{}\". The registered backends are {}.", name, names));
    }

    selected = found->second.get();
}

Backend& BackendRegistry::getSelected() const {
    if (selected == nullptr) {
        throw std::logic_error("No 3D physics backend is registered.");
    }

    return *selected;
}

bool BackendRegistry::contains(std::string_view name) const {
    return backends.find(name) != backends.end();
}

std::vector<std::string> BackendRegistry::getNames() const {
    std::vector<std::string> names;
    names.reserve(backends.size());
    for (const auto& [name, backend] : backends) {
        names.push_back(name);
    }

    return names;
}

} // namespace haylen::physics3d
