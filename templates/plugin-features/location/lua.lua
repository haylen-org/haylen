-- Asks for one fix of the location. Returns a platform call, whose `await` gives `{latitude, longitude, accuracy}` with the accuracy in meters.
function plugin.locate()
    return handle:call('locate', {})
end

-- Starts sending every change of the location to the listeners of `onLocated`, and `stopWatchingLocation` stops it.
function plugin.watchLocation()
    return handle:call('watchLocation', {})
end

function plugin.stopWatchingLocation()
    return handle:call('stopWatchingLocation', {})
end

-- Calls `listener(places)` once per frame with the list of the changes of the location that arrived since the frame before, each `{latitude, longitude, accuracy}`, and returns the connection that stops it.
function plugin.onLocated(listener)
    return handle:on('located', listener)
end
