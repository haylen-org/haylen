#import "platform/apple/AppleCutouts.hpp"

#if TARGET_OS_IOS && !TARGET_OS_MACCATALYST
namespace haylen::platform {

std::vector<math::Rect>& AppleCutouts::regions = *new std::vector<math::Rect>();

// The regions are in the points of the view, which covers the window, and the cutouts in framebuffer pixels. A release before iOS 27.1 reports none, and an SDK before iOS 27.1 declares no regions, so an engine built with it reports none.
void AppleCutouts::refresh([[maybe_unused]] UIView* view) {
    regions.clear();
#if __IPHONE_OS_VERSION_MAX_ALLOWED >= 270100
    if (@available(iOS 27.1, *)) {
        const auto scale = static_cast<float>(view.contentScaleFactor);
        for (UIViewReservedRegion* region in [view reservedRegionsOfKind:UIViewReservedRegionKind.occlusionRegionKind]) {
            if (region.active) {
                const CGRect frame = region.frame;
                regions.push_back({static_cast<float>(frame.origin.x) * scale, static_cast<float>(frame.origin.y) * scale, static_cast<float>(frame.size.width) * scale, static_cast<float>(frame.size.height) * scale});
            }
        }
    }
#endif
}

void AppleCutouts::get(std::vector<math::Rect>& cutouts) {
    cutouts.assign(regions.begin(), regions.end());
}

} // namespace haylen::platform
#endif
