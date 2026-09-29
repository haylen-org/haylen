#pragma once

#include <deque>
#include <functional>
#include <map>
#include <string>
#include <string_view>

namespace haylen::ai {

// Runs one named state at a time. A change leaves the current state and enters the next one, and changes requested while a transition runs wait for it to finish, in request order.
class StateMachine final {
  public:
    struct Callbacks {
        std::function<void()> enter;
        std::function<void(float deltaSeconds)> update;
        std::function<void()> exit;
    };

    void add(std::string name, Callbacks callbacks);
    void change(std::string_view name);
    void update(float deltaSeconds);

    [[nodiscard]] bool has(std::string_view name) const;

    // The current state is empty until the first change.
    [[nodiscard]] const std::string& getCurrent() const noexcept {
        return current;
    }
    [[nodiscard]] const std::string& getPrevious() const noexcept {
        return previous;
    }
    [[nodiscard]] float getElapsed() const noexcept {
        return elapsed;
    }

    std::function<void(std::string_view from, std::string_view to)> onChange;

  private:
    std::map<std::string, Callbacks, std::less<>> states;
    std::deque<std::string> pending;
    std::string current;
    std::string previous;
    float elapsed = 0.0F;
    bool transitioning = false;
};

} // namespace haylen::ai
