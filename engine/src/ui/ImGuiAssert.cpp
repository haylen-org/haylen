#include "ui/ImGuiAssert.hpp"

#include <stdexcept>
#include <string>

namespace haylen::ui {

void ImGuiAssert::fail(const char* expression, const char* file, int line) {
    throw std::logic_error(std::string("Dear ImGui check failed: ") + expression + " (" + file + ":" + std::to_string(line) + ")");
}

} // namespace haylen::ui
