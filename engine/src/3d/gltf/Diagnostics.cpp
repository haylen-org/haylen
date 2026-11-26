#include "3d/gltf/Diagnostics.hpp"

#include <string>

namespace haylen::gltf {

void Diagnostics::invalid(ReadError::Problem problem, std::string_view reason, std::string_view advice) const {
    throw ReadError(problem, "The glTF file \"" + file + "\" is not valid, since " + std::string(reason) + ". " + std::string(advice));
}

void Diagnostics::refuse(ReadError::Problem problem, std::string_view predicate, std::string_view advice) const {
    throw ReadError(problem, "The glTF file \"" + file + "\" " + std::string(predicate) + ". " + std::string(advice));
}

std::string Diagnostics::item(std::string_view kind, std::size_t index) {
    return std::string(kind) + " " + std::to_string(index);
}

std::string Diagnostics::item(std::string_view kind, std::size_t index, std::string_view owner) {
    return item(kind, index) + " of " + std::string(owner);
}

std::string Diagnostics::key(std::string_view name, std::string_view owner) {
    return "the key \"" + std::string(name) + "\" of " + std::string(owner);
}

} // namespace haylen::gltf
