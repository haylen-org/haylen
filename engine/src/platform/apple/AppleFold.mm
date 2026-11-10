#import "platform/apple/AppleFold.hpp"

#if TARGET_OS_IOS && !TARGET_OS_MACCATALYST
#include "haylen/math/Rect.hpp"
#include "platform/NativeWindow.hpp"

namespace haylen::platform {

std::optional<Fold> AppleFold::fold;
bool AppleFold::halfOpened = false;
UIView* AppleFold::followed = nil;

// A device without a hinge never calls the handler, and a release before iOS 27.1 has neither the interaction nor the regions. An SDK before iOS 27.1 declares neither, so an engine built with it reports no fold.
void AppleFold::attach([[maybe_unused]] UIView* view) {
#if __IPHONE_OS_VERSION_MAX_ALLOWED >= 270100
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
#endif
}

// The region is in the points of the view, which the fold keeps until `get` turns them into framebuffer pixels at the scale of the window then, since the pixel density cap changes that scale without a layout. A fold whose region has an area hides what lies under it, and a fold that is half opened or hides its region separates the window.
void AppleFold::refresh([[maybe_unused]] UIView* view) {
#if __IPHONE_OS_VERSION_MAX_ALLOWED >= 270100
    if (@available(iOS 27.1, *)) {
        fold = std::nullopt;
        for (UIViewReservedRegion* region in [view reservedRegionsOfKind:UIViewReservedRegionKind.divisionRegionKind]) {
            if (!region.active) {
                continue;
            }
            const CGRect frame = region.frame;
            const math::Rect bounds{static_cast<float>(frame.origin.x), static_cast<float>(frame.origin.y), static_cast<float>(frame.size.width), static_cast<float>(frame.size.height)};
            const bool occluding = frame.size.width > 0.0 && frame.size.height > 0.0;
            fold = Fold{.bounds = bounds, .axis = frame.size.height >= frame.size.width ? Fold::Axis::Vertical : Fold::Axis::Horizontal, .state = halfOpened ? Fold::State::HalfOpened : Fold::State::Flat, .separating = halfOpened || occluding, .occluding = occluding};
            return;
        }
    }
#endif
}

std::optional<Fold> AppleFold::get() noexcept {
    if (!fold) {
        return std::nullopt;
    }

    Fold scaled = *fold;
    const float scale = NativeWindow::getScale();
    scaled.bounds = {fold->bounds.x * scale, fold->bounds.y * scale, fold->bounds.width * scale, fold->bounds.height * scale};
    return scaled;
}

} // namespace haylen::platform
#endif
