#import "platform/apple/AppleCutouts.hpp"

#include "platform/NativeWindow.hpp"

#if TARGET_OS_IOS && !TARGET_OS_MACCATALYST
namespace haylen::platform {

std::vector<math::Rect>& AppleCutouts::regions = *new std::vector<math::Rect>();

// The regions are in the points of the view, which covers the window, and stay so until `get` turns them into framebuffer pixels at the scale of the window then, since the pixel density cap changes that scale without a layout. A release before iOS 27.1 reports none, and an SDK before iOS 27.1 declares no regions, so an engine built with it reports none.
void AppleCutouts::refresh([[maybe_unused]] UIView* view) {
    regions.clear();
#if __IPHONE_OS_VERSION_MAX_ALLOWED >= 270100
    if (@available(iOS 27.1, *)) {
        for (UIViewReservedRegion* region in [view reservedRegionsOfKind:UIViewReservedRegionKind.occlusionRegionKind]) {
            if (region.active) {
                const CGRect frame = region.frame;
                regions.push_back({static_cast<float>(frame.origin.x), static_cast<float>(frame.origin.y), static_cast<float>(frame.size.width), static_cast<float>(frame.size.height)});
            }
        }
    }
#endif
}

void AppleCutouts::get(std::vector<math::Rect>& cutouts) {
    const float scale = NativeWindow::getScale();
    cutouts.clear();
    for (const math::Rect& region : regions) {
        cutouts.push_back({region.x * scale, region.y * scale, region.width * scale, region.height * scale});
    }
}

} // namespace haylen::platform
#endif
