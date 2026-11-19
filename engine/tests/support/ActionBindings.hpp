#pragma once

#include <string_view>

#include "haylen/input/ActionMap.hpp"

namespace haylen::test {

// Parses the bindings that tests give their action maps, and throws for a binding that does not parse, so a test never runs with a binding it did not mean.
class ActionBindings final {
  public:
    [[nodiscard]] static input::ActionMap::Binding parse(std::string_view text);
};

} // namespace haylen::test
