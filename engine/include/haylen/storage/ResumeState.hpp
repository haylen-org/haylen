#pragma once

#include <optional>
#include <string>

#include "haylen/core/Json.hpp"

namespace haylen::storage {

class UserStorage;

// The state an app needs to come back where it was after the system ended it in the background, as mobile systems do to free memory. The app keeps its current state here, the state reaches user storage when the app goes to the background and leaves it when the app comes back or stops normally, so only a launch after the system ended the app finds it.
class ResumeState final {
  public:
    // What a launch found from the end of the run before: the value and how many seconds ago the app went to the background with it.
    struct Restored {
        core::Json value;
        double age = 0.0;
    };

    explicit ResumeState(UserStorage& storage, std::string file = "resume-state.json");

    // Reads what the run before left and removes it from user storage, so a later launch never finds it again. A damaged file is removed and throws.
    void load();

    // Keeps the value for the next time the app goes to the background, or nothing for `null`. Throws `std::invalid_argument` for a value that JSON text cannot hold.
    void set(core::Json value);

    // Writes the kept value to user storage, durably, with the time of the call.
    void save();

    // Removes the written value, as when the app comes back or stops normally.
    void discard();

    [[nodiscard]] const core::Json& getValue() const noexcept {
        return value;
    }
    [[nodiscard]] const std::optional<Restored>& getRestored() const noexcept {
        return restored;
    }

  private:
    UserStorage& userStorage;
    std::string path;
    core::Json value;
    std::optional<Restored> restored;
    bool written = false;
};

} // namespace haylen::storage
