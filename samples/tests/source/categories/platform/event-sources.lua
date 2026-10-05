-- The events that come from the platform, and why each platform never sends some of them, as the tables of "docs/lifecycle.md" describe.
local sources = {}

sources.events = {
    'appActive', 'appInactive', 'appBackground', 'appLowMemory', 'appQuitRequested',
    'windowResized', 'windowFocusGained', 'windowFocusLost', 'windowFullscreenChanged', 'windowOrientationChanged', 'windowFoldChanged', 'windowSafeAreaChanged', 'windowMoved', 'windowMonitorsChanged',
    'keyboardShown', 'keyboardHidden', 'networkOnline', 'networkOffline',
    'audioInterrupted', 'audioResumed', 'audioRouteChanged', 'systemThemeChanged', 'batteryChanged', 'gamepadConnected', 'gamepadDisconnected',
}

local kDesktopFold = 'A window of a desktop counts as landscape and reports no fold.'
local kNoDesktop = 'The window has no desktop.'
local kNoKeyboard = 'There is no keyboard on the screen.'
local kAlwaysFull = 'The app always fills its window.'

sources.never = {
    macos = {
        windowOrientationChanged = kDesktopFold, windowFoldChanged = kDesktopFold, keyboardShown = kNoKeyboard, keyboardHidden = kNoKeyboard,
        audioInterrupted = 'macOS does not interrupt the audio of apps.', audioResumed = 'macOS does not interrupt the audio of apps.',
    },
    windows = {
        appLowMemory = 'The engine follows no memory notification of Windows.', windowOrientationChanged = kDesktopFold, windowFoldChanged = kDesktopFold,
        keyboardShown = kNoKeyboard, keyboardHidden = kNoKeyboard, networkOnline = 'The engine follows no network state on Windows.', networkOffline = 'The engine follows no network state on Windows.',
        audioInterrupted = 'Windows does not interrupt the audio of apps.', audioResumed = 'Windows does not interrupt the audio of apps.',
    },
    linux = {
        appLowMemory = 'The engine follows no memory pressure of Linux.', windowOrientationChanged = kDesktopFold, windowFoldChanged = kDesktopFold,
        keyboardShown = kNoKeyboard, keyboardHidden = kNoKeyboard, networkOnline = 'The engine follows no network state on Linux.', networkOffline = 'The engine follows no network state on Linux.',
        audioInterrupted = 'Linux does not interrupt the audio of apps.', audioResumed = 'Linux does not interrupt the audio of apps.',
    },
    ios = {
        appQuitRequested = 'iOS apps never quit on request.', windowFullscreenChanged = kAlwaysFull, windowFoldChanged = 'No iPhone or iPad folds.',
        windowMoved = kNoDesktop, windowMonitorsChanged = kNoDesktop,
    },
    tvos = {
        appQuitRequested = 'tvOS apps never quit on request.', windowFullscreenChanged = 'The app always fills the TV in landscape.', windowOrientationChanged = 'The app always fills the TV in landscape.',
        windowFoldChanged = 'The app always fills the TV in landscape.', windowMoved = kNoDesktop, windowMonitorsChanged = kNoDesktop,
        keyboardShown = 'The keyboard of tvOS is a screen of the system over the app.', keyboardHidden = 'The keyboard of tvOS is a screen of the system over the app.', batteryChanged = 'A TV runs on mains power.',
    },
    catalyst = {
        appQuitRequested = 'Mac Catalyst ends the app on Quit without asking it.', windowFullscreenChanged = 'The window runs as the iPad window it is.', windowOrientationChanged = 'The window runs as the iPad window it is.',
        windowFoldChanged = 'No Mac folds.', windowMoved = 'UIKit places the window without telling apps where.', windowMonitorsChanged = 'UIKit places the window without telling apps where.',
        keyboardShown = kNoKeyboard, keyboardHidden = kNoKeyboard,
    },
    android = {
        appQuitRequested = 'Back on the root screen and the recent apps close the activity, which stops the app.', windowFullscreenChanged = kAlwaysFull,
        windowMoved = kNoDesktop, windowMonitorsChanged = kNoDesktop,
    },
    web = {
        appLowMemory = 'Browsers tell pages nothing about memory pressure.', appQuitRequested = 'A page that closes cannot wait for the app.',
        windowMoved = 'A page has no place on a desktop.', windowMonitorsChanged = 'A page has no place on a desktop.',
        audioInterrupted = 'Browsers suspend the audio of the page without telling it why.', audioResumed = 'Browsers suspend the audio of the page without telling it why.',
        audioRouteChanged = 'The audio of the page follows the browser without telling the page.',
    },
}

-- The headless platform has no device, so only the events that the engine or a test causes fire there.
sources.never.headless = {}
for _, name in ipairs(sources.events) do
    sources.never.headless[name] = 'The headless platform has no device.'
end

return sources
