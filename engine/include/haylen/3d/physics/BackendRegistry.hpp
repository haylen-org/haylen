#pragma once

#include <map>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "haylen/3d/physics/Backend.hpp"

namespace haylen::physics3d {

// The backends that can simulate the 3D worlds of the app by name, and the one that new worlds take. The first backend added is the selected one until `select` names another.
class BackendRegistry final {
  public:
    // Throws `std::invalid_argument` when a backend of the same name is registered already.
    void add(std::unique_ptr<Backend> backend);

    // Throws `std::invalid_argument` with the names of the registered backends when none has the name.
    void select(std::string_view name);

    // Throws `std::logic_error` when no backend is registered.
    [[nodiscard]] Backend& getSelected() const;

    [[nodiscard]] bool contains(std::string_view name) const;
    [[nodiscard]] std::vector<std::string> getNames() const;

  private:
    std::map<std::string, std::unique_ptr<Backend>, std::less<>> backends;
    Backend* selected = nullptr;
};

} // namespace haylen::physics3d
