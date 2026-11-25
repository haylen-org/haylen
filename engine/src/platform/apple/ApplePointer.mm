#import "platform/apple/ApplePointer.hpp"

#if TARGET_OS_IOS
#import <UIKit/UIKit.h>

#include <utility>

#include "platform/NativeWindow.hpp"

namespace haylen::platform {

std::function<void(SystemAnswer answer)>& ApplePointer::pending = *new std::function<void(SystemAnswer answer)>();
std::uint64_t ApplePointer::request = 0;
bool ApplePointer::observing = false;

bool ApplePointer::isControllable() {
    const UIUserInterfaceIdiom idiom = UIDevice.currentDevice.userInterfaceIdiom;
    return idiom == UIUserInterfaceIdiomPad || idiom == UIUserInterfaceIdiomMac;
}

void ApplePointer::refreshStyle() {
    UIWindow* window = (__bridge UIWindow*)NativeWindow::getHandle();
    for (id<UIInteraction> interaction in window.rootViewController.view.interactions) {
        if ([interaction isKindOfClass:UIPointerInteraction.class]) {
            [(UIPointerInteraction*)interaction invalidate];
        }
    }
}

bool ApplePointer::isLocked() {
    UIWindow* window = (__bridge UIWindow*)NativeWindow::getHandle();
    UIPointerLockState* state = window.windowScene.pointerLockState;
    return state != nil && state.isLocked;
}

void ApplePointer::settle(SystemAnswer answer) {
    std::function<void(SystemAnswer answer)> callback = std::exchange(pending, {});
    if (callback) {
        callback(std::move(answer));
    }
}

// UIKit locks the pointer after the view controller of the window asks for it, which SDL does as it turns its relative mode on, and reports each change of the lock through a notification of the scene.
void ApplePointer::awaitLock(std::function<void(SystemAnswer answer)> callback) {
    UIWindow* window = (__bridge UIWindow*)NativeWindow::getHandle();
    if (window.windowScene.pointerLockState == nil) {
        callback({.state = SystemAnswer::State::Unsupported, .message = "The scene of the app has no pointer to lock."});
        return;
    }

    settle({.state = SystemAnswer::State::Denied, .message = "A newer mode of the mouse replaced the lock of the pointer before the system locked it."});
    if (isLocked()) {
        callback({.state = SystemAnswer::State::Granted});
        return;
    }

    if (!observing) {
        observing = true;
        [NSNotificationCenter.defaultCenter addObserverForName:UIPointerLockStateDidChangeNotification
                                                        object:nil
                                                         queue:NSOperationQueue.mainQueue
                                                    usingBlock:^(NSNotification*) {
                                                      if (isLocked()) {
                                                          settle({.state = SystemAnswer::State::Granted});
                                                      }
                                                    }];
    }

    pending = std::move(callback);
    const std::uint64_t waiting = ++request;
    dispatch_after(dispatch_time(DISPATCH_TIME_NOW, static_cast<std::int64_t>(kLockSeconds * NSEC_PER_SEC)), dispatch_get_main_queue(), ^{
      if (waiting == request) {
          settle({.state = SystemAnswer::State::Denied, .message = "The system did not lock the pointer. An iPad or a Mac locks it only while a mouse or a trackpad is connected and the window of the app fills the screen with no other app beside it."});
      }
    });
}

} // namespace haylen::platform
#endif
