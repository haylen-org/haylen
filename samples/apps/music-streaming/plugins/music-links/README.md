# 🔗 Music Links

A local plugin of [Music Streaming](../../README.md) that registers the URL scheme `haylenmusic` with each system, so a link such as `haylenmusic://artist/12` opens the app from a cold start or reaches it while it runs. The link arrives as the event [`linkOpened`](../../../../../docs/lua-api/events.md#app-and-plugins), and the router of the app turns it into its screens, as the [navigation guide](../../../../../docs/ui/navigation.md#routes-for-links) describes. The app lists the plugin in its `app.json` with no parameters, and `haylen.py` brings each part into the project of each platform as the [plugin guide](../../../../../docs/platform/plugins.md) describes.

## Lua API

The plugin has no Lua API. Links reach the app through `linkOpened`, which `ui.router` hears by itself.

```lua
local ui = require('haylen.ui')

-- The router opens every link of the scheme, the one that launched the app and those that arrive while it runs.
local router = ui.router{tabs = tabs, schemes = {'haylenmusic'}, restore = true, routes = routes}
```

## Platforms

| Platform | What the plugin declares |
| --- | --- |
| iOS, Mac Catalyst, tvOS, visionOS, macOS | The `CFBundleURLTypes` key of the `Info.plist`, from the `apple` section of `plugin.json`. |
| Android | The module in `android/`, whose manifest adds an intent filter of the scheme to `dev.haylen.HaylenLinkActivity` of the `haylen-links` library. It has no plugin class, since the activity hands every link to the app. |
| Windows, Linux | The `desktop` section of `plugin.json`, whose scheme the installer of Windows and the desktop entry of Linux register. |
| Web, desktop player | None. A page is the link that opened it, and the desktop player registers no scheme. |

The plugin needs no setup on any platform. With the app installed, these commands open a link of the scheme:

| Platform | Command |
| --- | --- |
| iPhone and iPad simulator | `xcrun simctl openurl booted haylenmusic://artist/12` |
| macOS | `open haylenmusic://artist/12` |
| Android | `adb shell am start -a android.intent.action.VIEW -d haylenmusic://artist/12` |
| Windows | `start "" haylenmusic://artist/12` |
| Linux | `xdg-open haylenmusic://artist/12` |
