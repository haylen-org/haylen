#pragma once

#include <string_view>

namespace haylen::core {

// One way app code fails: the module `fault` of a recoverable app that raises from one kind of code, the message the error screen shows first and the line of the module it points at.
struct FailingCode {
    std::string_view name;
    std::string_view module;
    std::string_view message;
    int line = 0;
};

} // namespace haylen::core
