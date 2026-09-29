#import "platform/apple/CatalystWindow.hpp"

#if TARGET_OS_MACCATALYST
#include "sokol_app.h"

namespace haylen::platform {

void CatalystWindow::observe() {
    [NSNotificationCenter.defaultCenter addObserverForName:UISceneWillConnectNotification object:nil queue:nil usingBlock:^(NSNotification* notification) { resize((UIWindowScene*)notification.object); }];
}

// The system connects scenes of its own too, such as the one of the menus, while only the scene of the app holds its window. The system frame and the screen bounds are in the points of the Mac, like the window size of app.json, and macOS decides where the window finally goes.
void CatalystWindow::resize(UIWindowScene* scene) {
    if (![scene.session.role isEqualToString:UIWindowSceneSessionRoleApplication]) {
        return;
    }

    const sapp_desc desc = sapp_query_desc();
    const CGRect screen = scene.screen.bounds;
    const CGRect frame = CGRectMake(CGRectGetMidX(screen) - (desc.width / 2.0), CGRectGetMidY(screen) - (desc.height / 2.0), desc.width, desc.height);
    [scene requestGeometryUpdateWithPreferences:[[UIWindowSceneGeometryPreferencesMac alloc] initWithSystemFrame:frame] errorHandler:nil];
}

} // namespace haylen::platform
#endif
