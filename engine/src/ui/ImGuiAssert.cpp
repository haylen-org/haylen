#include "ui/ImGuiAssert.hpp"

#include <stdexcept>
#include <string>
#include <string_view>

namespace haylen::ui {

void ImGuiAssert::fail(const char* expression) {
    // Dear ImGui writes the reason of a check as a string after its condition, such as `(0) && "Missing End()"`.
    const std::string_view check(expression);
    const std::size_t close = check.rfind('"');
    const std::size_t open = close == std::string_view::npos || close == 0 ? std::string_view::npos : check.rfind('"', close - 1);
    if (open == std::string_view::npos) {
        throw std::logic_error("Dear ImGui refused a call that breaks one of its rules, such as a window, a table or an ID that is closed twice or never opened.");
    }

    const std::string reason(check.substr(open + 1, close - open - 1));
    throw std::logic_error("Dear ImGui refused a call out of order, with the reason \"" + reason + "\".");
}

} // namespace haylen::ui
