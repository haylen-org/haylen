#pragma once

namespace haylen::ui {

class ComponentRegistry;

// Registers every component kind the engine ships.
class BuiltInComponents final {
  public:
    static void registerAll(ComponentRegistry& registry);
};

} // namespace haylen::ui
