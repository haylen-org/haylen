#include "haylen/input/Gesture.hpp"

#include <algorithm>

namespace haylen::input {

std::string_view Gesture::typeName(Type value) noexcept {
    return std::ranges::find(kTypeNames, value, &std::pair<std::string_view, Type>::second)->first;
}

} // namespace haylen::input
