### plugin.showBanner(options), plugin.hideBanner() and plugin.onBannerTapped(listener)

Show and hide a native bar over the app with a text and a button, at the top or the bottom edge, as a native view of the platform that the overlay of the plugin places, so touches and clicks outside it reach the app. The table `options` takes `text`, `anchor`, `'top'` or `'bottom'` (the default), and `reserve`, which moves the safe area of the app out of the way of the bar. The button sends `{count}` to the listeners of `onBannerTapped`, whose connection `disconnect()` stops. A banner on the desktop library fails with the code `unsupported`.

```lua
local plugin = require('{{ID}}')
local scene = require('haylen.scene')

scene.push({
    enter = function(self)
        self.tapped = plugin.onBannerTapped(function(payload)
            print('Tapped ' .. payload.count .. ' times')
        end)
        plugin.showBanner({text = 'Hello from native code', anchor = 'top', reserve = true})
    end,
    exit = function(self)
        self.tapped:disconnect()
        plugin.hideBanner()
    end,
})
```
