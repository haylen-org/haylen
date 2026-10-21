#import "platform/apple/CatalystWindow.hpp"

#if TARGET_OS_MACCATALYST
#include "platform/Player.hpp"
#import "platform/apple/HaylenScreenSceneDelegate.h"

namespace haylen::platform {

void CatalystWindow::observe() {
    [NSNotificationCenter.defaultCenter addObserverForName:UISceneWillConnectNotification object:nil queue:nil usingBlock:^(NSNotification* notification) { resize((UIWindowScene*)notification.object); }];
}

// The system connects scenes of its own too, such as the one of the menus, and screens connect the scenes of their windows, while only the scene of the app holds its window. The system frame and the screen bounds are in the points of the Mac, like the window size of `app.json`, and macOS decides where the window finally goes.
void CatalystWindow::resize(UIWindowScene* scene) {
    if (![scene.session.role isEqualToString:UIWindowSceneSessionRoleApplication] || scene.session.configuration.delegateClass == HaylenScreenSceneDelegate.class) {
        return;
    }

    const math::Vec2 size = Player::getOpeningSize();
    const CGRect screen = scene.screen.bounds;
    const CGRect frame = CGRectMake(CGRectGetMidX(screen) - (size.x / 2.0), CGRectGetMidY(screen) - (size.y / 2.0), size.x, size.y);
    [scene requestGeometryUpdateWithPreferences:[[UIWindowSceneGeometryPreferencesMac alloc] initWithSystemFrame:frame] errorHandler:nil];
}

} // namespace haylen::platform
#endif
