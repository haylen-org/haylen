#pragma once

#include <string>

namespace haylen::test {

// Reads the variables of the environment that the tests take, such as the graphics API they run on, the same way on every C library.
class Environment final {
  public:
    Environment() = delete;

    // The value of the variable, or an empty string when it is not set.
    [[nodiscard]] static std::string get(const char* name);
};

} // namespace haylen::test
