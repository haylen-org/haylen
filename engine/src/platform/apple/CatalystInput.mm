#import "platform/apple/CatalystInput.hpp"

#if TARGET_OS_MACCATALYST
#include "haylen/platform/Event.hpp"
#import "platform/apple/HaylenPointer.h"
#include "platform/sokol/SokolRuntime.hpp"
#include "sokol_app.h"

namespace haylen::platform {

math::Vec2 CatalystInput::pointer{};

void CatalystInput::observe() {
    static HaylenPointer* const tracker = [HaylenPointer new];
    NSNotificationCenter* center = NSNotificationCenter.defaultCenter;
    [center addObserverForName:GCMouseDidConnectNotification object:nil queue:NSOperationQueue.mainQueue usingBlock:^(NSNotification* notification) { observeMouse(notification.object); }];
    [center addObserverForName:UIWindowDidBecomeKeyNotification
                        object:nil
                         queue:NSOperationQueue.mainQueue
                    usingBlock:^(NSNotification* notification) {
                      if (!sapp_isvalid() || notification.object != (__bridge UIWindow*)sapp_ios_get_window()) {
                          return;
                      }
                      UIView* view = ((UIWindow*)notification.object).rootViewController.view;
                      for (UIGestureRecognizer* recognizer in view.gestureRecognizers) {
                          if ([recognizer isKindOfClass:UIHoverGestureRecognizer.class]) {
                              return;
                          }
                      }
                      [view addGestureRecognizer:[[UIHoverGestureRecognizer alloc] initWithTarget:tracker action:@selector(hover:)]];
                      for (GCMouse* mouse in GCMouse.mice) {
                          observeMouse(mouse);
                      }
                    }];
}

void CatalystInput::trackPointer(UIHoverGestureRecognizer* recognizer) {
    const CGPoint location = [recognizer locationInView:recognizer.view];
    const float scale = sapp_dpi_scale();
    pointer = {static_cast<float>(location.x) * scale, static_cast<float>(location.y) * scale};
    const bool inside = recognizer.state == UIGestureRecognizerStateBegan || recognizer.state == UIGestureRecognizerStateChanged;
    SokolRuntime::handleEvent({.type = inside ? Event::Type::MouseMove : Event::Type::MouseLeave, .position = pointer});
}

void CatalystInput::observeMouse(GCMouse* mouse) {
    GCMouseInput* state = mouse.mouseInput;
    if (state.rightButton != nil) {
        observeMouseButton(state.rightButton, input::MouseButton::Right);
    }
    if (state.middleButton != nil) {
        observeMouseButton(state.middleButton, input::MouseButton::Middle);
    }
    state.scroll.valueChangedHandler = ^(GCControllerDirectionPad*, float x, float y) { SokolRuntime::handleEvent({.type = Event::Type::MouseScroll, .scroll = {x, y}}); };
}

void CatalystInput::observeMouseButton(GCControllerButtonInput* button, input::MouseButton which) {
    button.pressedChangedHandler = ^(GCControllerButtonInput*, float, BOOL pressed) { SokolRuntime::handleEvent({.type = pressed ? Event::Type::MouseDown : Event::Type::MouseUp, .mouseButton = which, .position = pointer}); };
}

} // namespace haylen::platform
#endif
