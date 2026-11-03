#import "platform/apple/AppleOutputs.hpp"

#if TARGET_OS_IOS && !TARGET_OS_MACCATALYST
#include <cmath>
#include <string>

#include "haylen/platform/Event.hpp"
#include "platform/Player.hpp"
#import "platform/apple/AppleDisplays.hpp"
#import "platform/apple/HaylenDisplaySceneDelegate.h"
#import "platform/apple/HaylenOutputView.h"

namespace haylen::platform {

std::map<std::uint32_t, AppleOutputs::Entry>& AppleOutputs::entries = *new std::map<std::uint32_t, AppleOutputs::Entry>();
NSObject* AppleOutputs::accessory = nil;

void AppleOutputs::announce(std::uint32_t number) {
    Player::postEvent({.type = Event::Type::OutputChanged, .output = number});
}

// The scene that UIKit connects for an external display holds the window of its output. A display that only Stage Manager extends to has no such scene.
void AppleOutputs::open(std::uint32_t number, std::string_view display, const gpu::Surface& surface) {
    Entry& entry = entries[number];
    for (UIScene* scene in UIApplication.sharedApplication.connectedScenes) {
        if ([scene.session.role isEqualToString:UIWindowSceneSessionRoleExternalDisplayNonInteractive] && std::string_view(AppleDisplays::getId(static_cast<UIWindowScene*>(scene).screen).UTF8String) == display) {
            entry.scene = static_cast<UIWindowScene*>(scene);
        }
    }
    if (entry.scene == nil) {
        entry.report = {.ending = Output::Ending::Unsupported, .reason = "UIKit shows an output only on an external display whose scene it connected, and the display \"" + std::string(display) + "\" has none."};
        announce(number);
        return;
    }

    UIScreen* screen = entry.scene.screen;
    entry.window = [[UIWindow alloc] initWithWindowScene:entry.scene];
    entry.view = [[HaylenOutputView alloc] initWithOutput:number frame:entry.window.bounds];
    entry.view.contentScaleFactor = screen.scale;
    entry.view.backgroundColor = UIColor.blackColor;
    UIViewController* controller = [UIViewController new];
    controller.view = entry.view;
    entry.window.rootViewController = controller;
    entry.window.hidden = NO;

    entry.report.surface = surface;
    entry.report.surface->kind = gpu::Surface::Kind::MetalLayer;
    entry.report.surface->layer = (__bridge void*)entry.view.layer;
    entry.report.surface->transparent = false;
    resize(number);
}

void AppleOutputs::close(std::uint32_t number) {
    const auto found = entries.find(number);
    if (found == entries.end()) {
        return;
    }
    found->second.window.hidden = YES;
    entries.erase(found);
}

Host::OutputReport AppleOutputs::getReport(std::uint32_t number) {
    const auto found = entries.find(number);
    return found != entries.end() ? found->second.report : Host::OutputReport{.ending = Output::Ending::App};
}

// The drawable covers the view in the pixels of its screen.
void AppleOutputs::resize(std::uint32_t number) {
    const auto found = entries.find(number);
    if (found == entries.end() || !found->second.report.surface) {
        return;
    }
    Entry& entry = found->second;
    const CGFloat scale = entry.view.contentScaleFactor;
    const int width = static_cast<int>(std::lround(entry.view.bounds.size.width * scale));
    const int height = static_cast<int>(std::lround(entry.view.bounds.size.height * scale));
    if (width == entry.report.surface->width && height == entry.report.surface->height) {
        return;
    }
    entry.report.surface->width = width;
    entry.report.surface->height = height;
    entry.report.scale = static_cast<float>(scale);
    announce(number);
}

// An SDK before iOS 27 declares no scene accessory, so an engine built with it leaves the scene of an external display to connect as earlier releases do.
void AppleOutputs::registerAccessory([[maybe_unused]] UIViewController* controller) {
#if __IPHONE_OS_VERSION_MAX_ALLOWED >= 270000
    if (@available(iOS 27.0, *)) {
        if (accessory != nil) {
            return;
        }
        UISceneConfiguration* configuration = [[UISceneConfiguration alloc] initWithName:@"HaylenDisplay" sessionRole:UIWindowSceneSessionRoleExternalDisplayNonInteractive];
        configuration.delegateClass = HaylenDisplaySceneDelegate.class;
        accessory = [controller registerSceneAccessory:[UISceneAccessory externalNonInteractiveSceneAccessoryWithConfiguration:configuration]];
    }
#endif
}

void AppleOutputs::disconnect(UIScene* scene) {
    for (auto& [number, entry] : entries) {
        if (entry.scene == scene && !entry.report.ending) {
            entry.window.hidden = YES;
            entry.window = nil;
            entry.view = nil;
            entry.report = {.ending = Output::Ending::Disconnected};
            announce(number);
        }
    }
}

} // namespace haylen::platform
#endif
