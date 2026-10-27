#include "support/Environment.hpp"

#include <cstdlib>
#include <memory>

namespace haylen::test {

std::string Environment::get(const char* name) {
#if defined(_MSC_VER)
    // The C library of MSVC deprecates `std::getenv` for `_dupenv_s`, which hands over a copy that the caller frees.
    char* value = nullptr;
    std::size_t length = 0;
    if (_dupenv_s(&value, &length, name) != 0 || value == nullptr) {
        return {};
    }
    const std::unique_ptr<char, decltype(&std::free)> owned(value, &std::free);
    return owned.get();
#else
    const char* value = std::getenv(name);
    return value == nullptr ? std::string() : std::string(value);
#endif
}

} // namespace haylen::test
