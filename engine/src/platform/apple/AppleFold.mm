#import "platform/apple/AppleFold.hpp"

#if TARGET_OS_IOS && !TARGET_OS_MACCATALYST
#include "haylen/math/Rect.hpp"

namespace haylen::platform {

std::optional<Fold> AppleFold::fold;
bool AppleFold::halfOpened = false;
UIView* AppleFold::followed = nil;

// A device without a hinge never calls the handler, and a release before iOS 27.1 has neither the interaction nor the regions.
void AppleFold::attach(UIView* view) {
    if (@available(iOS 27.1, *)) {
        if (followed == view) {
            return;
        }
        followed = view;
        // clang-format off
        UIHingeInteraction* hinge = [[UIHingeInteraction alloc] initWithUpdateHandler:^(UIHingeInteraction*, UIHingeInteractionUpdate* update) {
            halfOpened = update.hinge != nil && update.hinge.status == UIHingeStatusPartiallyOpen;
            refresh(followed);
        }];
        // clang-format on
        [view addInteraction:hinge];
        refresh(view);
    }
}

// The region is in the points of the view, and the fold in framebuffer pixels. A fold whose region has an area hides what lies under it, and a fold that is half opened or hides its region separates the window.
void AppleFold::refresh(UIView* view) {
    if (@available(iOS 27.1, *)) {
        fold = std::nullopt;
        for (UIViewReservedRegion* region in [view reservedRegionsOfKind:UIViewReservedRegionKind.divisionRegionKind]) {
            if (!region.active) {
                continue;
            }
            const CGRect frame = region.frame;
            const auto scale = static_cast<float>(view.contentScaleFactor);
            const math::Rect bounds{static_cast<float>(frame.origin.x) * scale, static_cast<float>(frame.origin.y) * scale, static_cast<float>(frame.size.width) * scale, static_cast<float>(frame.size.height) * scale};
            const bool occluding = frame.size.width > 0.0 && frame.size.height > 0.0;
            fold = Fold{.bounds = bounds, .axis = frame.size.height >= frame.size.width ? Fold::Axis::Vertical : Fold::Axis::Horizontal, .state = halfOpened ? Fold::State::HalfOpened : Fold::State::Flat, .separating = halfOpened || occluding, .occluding = occluding};
            return;
        }
    }
}

std::optional<Fold> AppleFold::get() noexcept {
    return fold;
}

} // namespace haylen::platform
#endif
