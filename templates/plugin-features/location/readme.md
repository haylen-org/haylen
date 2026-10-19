### plugin.locate()

Asks for one fix of the location and gives `{latitude, longitude, accuracy}`, with the accuracy in meters. The platform asks the person for the location while the app is in use the first time, and the call fails with the code `permissionDenied` when the person refuses, with `locationUnknown` when the device finds no fix, and with `unsupported` on a device whose location is off and on the desktop library.

```lua
local async = require('async')
local plugin = require('{{ID}}')

async.spawn(function()
    local place, err = plugin.locate():await()
    print(place and string.format('%.5f, %.5f', place.latitude, place.longitude) or err.message)
end)
```

### plugin.watchLocation(), plugin.stopWatchingLocation() and plugin.onLocated(listener)

Start and stop sending every change of the location. The changes are batched events, so `listener(places)` runs once per frame with the list of the changes that arrived since the frame before, each `{latitude, longitude, accuracy}`, and `onLocated` returns the connection of the listener, whose `disconnect()` stops it. Apple TV gives one fix but no updates, so watching fails there with the code `unsupported`.

```lua
local plugin = require('{{ID}}')
local scene = require('haylen.scene')

scene.push({
    enter = function(self)
        self.located = plugin.onLocated(function(places)
            self.place = places[#places]
        end)
        plugin.watchLocation()
    end,
    exit = function(self)
        self.located:disconnect()
        plugin.stopWatchingLocation()
    end,
})
```
