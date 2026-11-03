#pragma once

#include <TargetConditionals.h>

#if TARGET_OS_IOS && !TARGET_OS_MACCATALYST
#import <UIKit/UIKit.h>

#include <vector>

#include "haylen/math/Rect.hpp"

namespace haylen::platform {

// The cutouts of the screen over the window of an iPhone, such as the dynamic island or the camera of the foldable iPhone, which UIKit reports from iOS 27.1 as the regions of a view that elements of the screen occlude. The view controller of the window reads them again with every layout of its view, on the main thread, which is the frame thread of SDL.
class AppleCutouts final {
  public:
    static void refresh(UIView* view);
    static void get(std::vector<math::Rect>& cutouts);

  private:
    static std::vector<math::Rect>& regions;
};

} // namespace haylen::platform
#endif
