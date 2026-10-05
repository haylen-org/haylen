#include "ui/components/collections/LinearLayout.hpp"

namespace haylen::ui {

void LinearLayout::arrange(const Input& input, std::size_t) {
    count = input.count;
    crossLength = input.crossLength;
}

} // namespace haylen::ui
