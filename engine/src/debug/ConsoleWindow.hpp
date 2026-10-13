#pragma once

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

#include "haylen/debug/Console.hpp"

struct ImGuiInputTextCallbackData;

namespace haylen::debug {

// Draws the developer console with ImGui: the output, the line being typed with its history and completion, and buttons for every command and every line of the history, so a touch screen, a gamepad or a remote runs commands without a keyboard.
class ConsoleWindow final {
  public:
    // Draws the window and returns `false` once the player closed it.
    bool draw(Console& console);

    // Puts the focus on the line the next time the window draws.
    void focusLine() noexcept {
        focusPending = true;
    }

  private:
    static int edit(ImGuiInputTextCallbackData* data);

    void drawOutput(const Console& console);
    void drawCommands(Console& console);
    void drawHistory(Console& console);
    void submit(Console& console);
    [[nodiscard]] std::string complete(std::string_view typed);
    [[nodiscard]] std::string recall(int direction);

    const Console* shown = nullptr;
    std::string line;
    std::vector<std::string> candidates;
    std::size_t recalled = 0;
    bool focusPending = true;
    bool scrollPending = false;
    std::size_t shownLines = 0;
};

} // namespace haylen::debug
