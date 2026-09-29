#include "haylen/input/Gesture.hpp"

namespace haylen::input {

std::string_view Gesture::typeName(Type value) noexcept {
    switch (value) {
    case Type::Tap:
        return "tap";
    case Type::DoubleTap:
        return "double_tap";
    case Type::LongPress:
        return "long_press";
    case Type::Swipe:
        return "swipe";
    case Type::Pinch:
        return "pinch";
    }
    return "tap";
}

} // namespace haylen::input
