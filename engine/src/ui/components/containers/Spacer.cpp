#include "ui/components/containers/Spacer.hpp"

#include "haylen/ui/Context.hpp"

namespace haylen::ui {

math::Vec2 Spacer::measureContent(Context&, float) {
    return {};
}

} // namespace haylen::ui
