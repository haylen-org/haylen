#pragma once

#include <TargetConditionals.h>

#if TARGET_OS_IOS && !TARGET_OS_MACCATALYST
#import <UIKit/UIKit.h>

#include <optional>

#include "haylen/platform/Fold.hpp"

namespace haylen::platform {

// The fold of a foldable iPhone across the window of the app, from iOS 27.1 on: the division region that UIKit reserves in the view of the window where the screen folds, and the status of the hinge, which a hinge interaction on the view reports. The view controller of the window reads the region again after every layout, so the frame thread reads the fold without asking UIKit. Everything runs on the main thread, which is the frame thread.
class AppleFold final {
  public:
    // Follows the hinge of the device from the view of the window of the app.
    static void attach(UIView* view);

    // Reads the division region of the view again, once its layout changed.
    static void refresh(UIView* view);

    [[nodiscard]] static std::optional<Fold> get() noexcept;

  private:
    static std::optional<Fold> fold;
    static bool halfOpened;
    static UIView* followed;
};

} // namespace haylen::platform
#endif
