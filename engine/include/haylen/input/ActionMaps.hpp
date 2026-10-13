#pragma once

#include <cstddef>
#include <memory>
#include <vector>

#include "haylen/input/ActionMap.hpp"

namespace haylen::input {

class Input;
class VirtualInput;

// Holds the default action map of the app and the maps it creates for its players, such as one map per seat of a local multiplayer game, each reading its own devices. Every live map updates once per frame and at the start of every fixed step, and takes the presses the UI captures. A created map lives while something holds it, and the container forgets it at the next update after the last holder lets go.
class ActionMaps final {
  public:
    ActionMaps();

    [[nodiscard]] const std::shared_ptr<ActionMap>& getDefault() const noexcept {
        return defaultMap;
    }
    [[nodiscard]] std::shared_ptr<ActionMap> create(const ActionMap::Devices& devices);

    // Counts the default map and the created maps that were alive at the last update.
    [[nodiscard]] std::size_t size() const noexcept {
        return created.size() + 1;
    }

    void setCapture(const ActionMap::Capture& value) noexcept;
    void update(const Input& input, const VirtualInput& virtualInput, bool blocked);
    void beginStep(float stepSeconds) noexcept;

  private:
    std::shared_ptr<ActionMap> defaultMap;
    std::vector<std::weak_ptr<ActionMap>> created;
    ActionMap::Capture capture;
};

} // namespace haylen::input
