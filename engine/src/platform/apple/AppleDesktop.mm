#import "platform/apple/AppleDesktop.hpp"

#if TARGET_OS_OSX
#include <algorithm>
#include <utility>

#include "haylen/math/Geometry.hpp"
#include "haylen/platform/Event.hpp"
#include "platform/NativeWindow.hpp"
#include "platform/Player.hpp"

namespace haylen::platform {

NSEvent* AppleDesktop::lastMouseDown = nil;
Window::Passthrough AppleDesktop::passthrough = Window::Passthrough::Off;
std::vector<math::Polygon::Outline>& AppleDesktop::regions = *new std::vector<math::Polygon::Outline>();
math::Vec2 AppleDesktop::pointer{};
bool AppleDesktop::pointerInside = false;
bool AppleDesktop::dragging = false;

NSWindow* AppleDesktop::getWindow() {
    return (__bridge NSWindow*)NativeWindow::getHandle();
}

// The primary screen is the first screen, the one whose bottom left corner is the origin of AppKit.
math::Rect AppleDesktop::toDesktop(NSRect rect) {
    const CGFloat top = NSMaxY(NSScreen.screens.firstObject.frame);
    return {static_cast<float>(rect.origin.x), static_cast<float>(top - NSMaxY(rect)), static_cast<float>(rect.size.width), static_cast<float>(rect.size.height)};
}

NSRect AppleDesktop::toScreen(const math::Rect& rect) {
    const CGFloat top = NSMaxY(NSScreen.screens.firstObject.frame);
    return NSMakeRect(rect.x, top - rect.y - rect.height, rect.width, rect.height);
}

void AppleDesktop::setStyle(const WindowStyle& value) {
    NSWindow* window = getWindow();

    // Native fullscreen owns the style mask until the window leaves it, and AppKit then brings this one back.
    if ((window.styleMask & NSWindowStyleMaskFullScreen) == 0) {
        NSWindowStyleMask mask = value.decorated ? (NSWindowStyleMaskTitled | NSWindowStyleMaskClosable | NSWindowStyleMaskMiniaturizable) : NSWindowStyleMaskBorderless;
        if (value.resizable) {
            mask |= NSWindowStyleMaskResizable;
        }
        if (mask != window.styleMask) {
            // A new style mask keeps the outer frame, so the content area goes back where it was.
            const NSRect content = [window contentRectForFrameRect:window.frame];
            window.styleMask = mask;
            [window setFrame:[window frameRectForContentRect:content] display:YES];
        }
    }

    // An opaque window gets the background, the shadow and the opaque layer of a normal window, so its title bar and its content never show what is behind them.
    window.opaque = !value.transparent;
    window.backgroundColor = value.transparent ? NSColor.clearColor : NSColor.windowBackgroundColor;
    window.hasShadow = !value.transparent;
    window.contentView.layer.opaque = !value.transparent;

    // A window above the others also stays on every space, like the panels of the system.
    constexpr NSWindowCollectionBehavior kEverySpace = NSWindowCollectionBehaviorCanJoinAllSpaces | NSWindowCollectionBehaviorStationary;
    window.level = value.alwaysOnTop ? NSFloatingWindowLevel : NSNormalWindowLevel;
    window.collectionBehavior = value.alwaysOnTop ? (window.collectionBehavior | kEverySpace) : (window.collectionBehavior & ~kEverySpace);

    // An accessory app has neither a Dock icon nor a menu bar.
    const NSApplicationActivationPolicy policy = value.shownInTaskbar ? NSApplicationActivationPolicyRegular : NSApplicationActivationPolicyAccessory;
    if (NSApp.activationPolicy != policy) {
        NSApp.activationPolicy = policy;
    }
}

math::Rect AppleDesktop::getFrame() {
    NSWindow* window = getWindow();
    return toDesktop([window contentRectForFrameRect:window.frame]);
}

void AppleDesktop::setFrame(const math::Rect& value) {
    NSWindow* window = getWindow();
    [window setFrame:[window frameRectForContentRect:toScreen(value)] display:YES];
}

// A display is known by the vendor, the model and the serial number of its monitor, which stay the same across runs and reconnections, and two monitors that report the same ones, such as two of a model without serial numbers, count up in the order of the screens.
std::string AppleDesktop::getDisplayId(NSScreen* screen) {
    const CGDirectDisplayID display = [screen.deviceDescription[@"NSScreenNumber"] unsignedIntValue];
    NSString* identity = [NSString stringWithFormat:@"%04x-%04x-%08x", CGDisplayVendorNumber(display), CGDisplayModelNumber(display), CGDisplaySerialNumber(display)];
    NSUInteger twins = 0;
    for (NSScreen* other in NSScreen.screens) {
        if (other == screen) {
            break;
        }
        const CGDirectDisplayID earlier = [other.deviceDescription[@"NSScreenNumber"] unsignedIntValue];
        if (CGDisplayVendorNumber(earlier) == CGDisplayVendorNumber(display) && CGDisplayModelNumber(earlier) == CGDisplayModelNumber(display) && CGDisplaySerialNumber(earlier) == CGDisplaySerialNumber(display)) {
            ++twins;
        }
    }
    return twins == 0 ? identity.UTF8String : [identity stringByAppendingFormat:@"-%lu", static_cast<unsigned long>(twins + 1)].UTF8String;
}

// The modes that the desktop runs in, including the ones of low resolution that macOS hides from its settings, each one once.
std::vector<Display::Mode> AppleDesktop::getModes(CGDirectDisplayID display) {
    NSDictionary* options = @{(__bridge NSString*)kCGDisplayShowDuplicateLowResolutionModes : @YES};
    const CFArrayRef all = CGDisplayCopyAllDisplayModes(display, (__bridge CFDictionaryRef)options);
    std::vector<Display::Mode> modes;
    for (CFIndex index = 0, count = all != nullptr ? CFArrayGetCount(all) : 0; index < count; ++index) {
        const auto mode = static_cast<CGDisplayModeRef>(const_cast<void*>(CFArrayGetValueAtIndex(all, index)));
        if (!CGDisplayModeIsUsableForDesktopGUI(mode)) {
            continue;
        }
        const Display::Mode found{.width = static_cast<int>(CGDisplayModeGetPixelWidth(mode)), .height = static_cast<int>(CGDisplayModeGetPixelHeight(mode)), .refreshRate = static_cast<float>(CGDisplayModeGetRefreshRate(mode)), .scale = static_cast<float>(CGDisplayModeGetPixelWidth(mode)) / static_cast<float>(CGDisplayModeGetWidth(mode))};
        if (std::ranges::find(modes, found) == modes.end()) {
            modes.push_back(found);
        }
    }
    if (all != nullptr) {
        CFRelease(all);
    }
    return modes;
}

// The visible frame of a screen leaves out the menu bar and the Dock. The panels of laptops report no refresh rate in their modes, so the rate of a display is the most frames per second that its screen shows.
std::vector<Display> AppleDesktop::getDisplays() {
    NSArray<NSScreen*>* screens = NSScreen.screens;
    std::vector<Display> displays;
    displays.reserve(screens.count);
    for (NSScreen* screen in screens) {
        const CGDirectDisplayID display = [screen.deviceDescription[@"NSScreenNumber"] unsignedIntValue];
        std::vector<Display::Mode> modes = getModes(display);
        const float rate = static_cast<float>(screen.maximumFramesPerSecond);
        for (Display::Mode& mode : modes) {
            mode.refreshRate = mode.refreshRate > 0.0F ? mode.refreshRate : rate;
        }
        displays.push_back({.id = getDisplayId(screen), .name = screen.localizedName.UTF8String, .bounds = toDesktop(screen.frame), .workArea = toDesktop(screen.visibleFrame), .scale = static_cast<float>(screen.backingScaleFactor), .refreshRate = rate, .primary = screen == screens.firstObject, .modes = std::move(modes)});
    }
    return displays;
}

// AppKit gives a window that does not show yet no screen, so the display of the window is the screen that holds most of its frame, as AppKit decides once it shows, or the primary screen while it is off every screen.
std::string AppleDesktop::getWindowDisplayId() {
    const math::Rect frame = getFrame();
    NSScreen* holding = NSScreen.screens.firstObject;
    float largest = 0.0F;
    for (NSScreen* screen in NSScreen.screens) {
        const float overlap = toDesktop(screen.frame).intersection(frame).getArea();
        if (overlap > largest) {
            largest = overlap;
            holding = screen;
        }
    }
    return getDisplayId(holding);
}

void AppleDesktop::setPassthrough(Window::Passthrough mode, std::span<const math::Polygon::Outline> value) {
    passthrough = mode;
    regions.assign(value.begin(), value.end());
    if (mode == Window::Passthrough::Off) {
        getWindow().ignoresMouseEvents = NO;
    }
}

// The window server moves the window from the press of the button until its release, which then never reaches the view.
void AppleDesktop::startDrag() {
    if (lastMouseDown == nil || (NSEvent.pressedMouseButtons & 1U) == 0) {
        return;
    }
    [getWindow() performWindowDragWithEvent:lastMouseDown];
    dragging = true;
}

void AppleDesktop::watch() {
    NSWindow* window = getWindow();
    NSNotificationCenter* center = NSNotificationCenter.defaultCenter;
    // AppKit places windows by their bottom left corner, so a window that grows or shrinks at its top also moves in desktop points.
    [center addObserverForName:NSWindowDidMoveNotification object:window queue:nil usingBlock:^(NSNotification*) { Player::postEvent({.type = Event::Type::WindowMoved}); }];
    [center addObserverForName:NSWindowDidResizeNotification object:window queue:nil usingBlock:^(NSNotification*) { Player::postEvent({.type = Event::Type::WindowMoved}); }];
    [center addObserverForName:NSApplicationDidChangeScreenParametersNotification object:nil queue:nil usingBlock:^(NSNotification*) { Player::postEvent({.type = Event::Type::DisplaysChanged}); }];

    // A drag of the window starts from the press that is still down, and a release that reaches the app ends it the usual way.
    // clang-format off
    [NSEvent addLocalMonitorForEventsMatchingMask:NSEventMaskLeftMouseDown | NSEventMaskLeftMouseUp handler:^NSEvent*(NSEvent* event) {
        if (event.type == NSEventTypeLeftMouseDown) {
            lastMouseDown = event;
        } else {
            dragging = false;
        }
        return event;
    }];
    // clang-format on
}

void AppleDesktop::update() {
    NSWindow* window = getWindow();
    const NSRect content = [window contentRectForFrameRect:window.frame];
    const NSPoint mouse = NSEvent.mouseLocation;
    const bool inside = NSPointInRect(mouse, content);
    const CGFloat scale = NativeWindow::getFramebufferSize().x / content.size.width;
    const math::Vec2 pixel{static_cast<float>((mouse.x - content.origin.x) * scale), static_cast<float>((NSMaxY(content) - mouse.y) * scale)};
    const bool pressed = NSEvent.pressedMouseButtons != 0;

    if (dragging && (NSEvent.pressedMouseButtons & 1U) == 0) {
        finishDrag(pixel);
    }

    // The window keeps the mouse it has while a button is down, so a press that starts over the app also ends there.
    if (passthrough != Window::Passthrough::Off && !pressed) {
        window.ignoresMouseEvents = passthrough == Window::Passthrough::Whole || !inside || !isOverRegion(pixel);
    }

    // AppKit moves the mouse only over a key window that takes the mouse, so the app hears the other moves from here.
    if (window.ignoresMouseEvents || (!window.canBecomeKeyWindow && !pressed)) {
        followPointer(pixel, inside);
    }
}

bool AppleDesktop::isOverRegion(math::Vec2 pixel) {
    return std::ranges::any_of(regions, [pixel](const math::Polygon::Outline& region) { return math::Geometry::contains(region, pixel); });
}

void AppleDesktop::followPointer(math::Vec2 pixel, bool inside) {
    if (inside && (!pointerInside || pixel != pointer)) {
        Player::handleEvent({.type = Event::Type::MouseMove, .position = pixel});
    } else if (!inside && pointerInside) {
        Player::handleEvent({.type = Event::Type::MouseLeave, .position = pixel});
    }
    pointer = pixel;
    pointerInside = inside;
}

void AppleDesktop::finishDrag(math::Vec2 pixel) {
    dragging = false;
    Player::handleEvent({.type = Event::Type::MouseUp, .mouseButton = input::MouseButton::Left, .position = pixel});
}

} // namespace haylen::platform
#endif
