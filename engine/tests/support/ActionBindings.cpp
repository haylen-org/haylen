#include "support/ActionBindings.hpp"

#include <optional>
#include <stdexcept>
#include <string>

namespace haylen::test {

input::ActionMap::Binding ActionBindings::parse(std::string_view text) {
    const std::optional<input::ActionMap::Binding> binding = input::ActionMap::Binding::parse(text);
    if (!binding) {
        throw std::runtime_error("The binding \"" + std::string(text) + "\" does not parse.");
    }

    return *binding;
}

} // namespace haylen::test
