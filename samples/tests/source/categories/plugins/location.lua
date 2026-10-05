-- The simulated location and map of an SDK: the native part asks the platform where the device is, after the permission of the person, and shows a native map over the app at that place. Swift uses Core Location and a MapKit view, Kotlin "LocationManager" and a view that draws the map, and JavaScript the Geolocation API and a canvas over the page.
local haylen = require('haylen')
local ui = require('haylen.ui')

local DemoTest = require('categories.plugins.demo-test')
local demo = require('native-demo')

local Location = haylen.class('Location', DemoTest)

Location.locationName = 'The platform tells where the device is'
Location.mapName = 'A native map shows over the app'

-- The place the map shows until the location answers, a hill above a bay.
Location.place = {latitude = -22.9519, longitude = -43.2105}

function Location:enter()
    self:frame{
        hint = 'Find the location, then show the map at it.',
        focus = 'locate',
        controls = {
            ui.button{id = 'locate', text = 'Find the location', variant = 'primary', onClick = function() self:locate() end},
            ui.button{id = 'map', text = 'Show the map', onClick = function() self:toggleMap() end},
            ui.label{text = 'Core Location and MapKit on Apple platforms, "LocationManager" and a drawn map on Android, and the Geolocation API and a drawn map on the web. The system asks the person for the location the first time.', color = 'textMuted', font = 'caption'},
        },
        code = "local place = demo.location():await()\ndemo.showMap('bottom', place.latitude, place.longitude):await()",
    }
end

function Location:exit()
    if self.mapShown then
        demo.removeMap()
    end
    Location.super.exit(self)
end

function Location:locate()
    self:act(function()
        self.results:set('location', 'waiting', Location.locationName, 'The platform looks for the device.')
        local place, err = demo.location():await()
        if err then
            self.results:failure('location', Location.locationName, err)
            return
        end
        self.place = place
        self.results:set('location', 'pass', Location.locationName, string.format('%s found %.5f, %.5f within %.0f meters.', place.language, place.latitude, place.longitude, place.accuracy))
    end)
end

function Location:toggleMap()
    if self.mapShown then
        self.mapShown = false
        demo.removeMap()
        self:set('map', {text = 'Show the map'})
        return
    end
    self:act(function()
        local place = self.place or Location.place
        local shown, err = demo.showMap('bottom', place.latitude, place.longitude):await()
        if err then
            self.results:failure('map', Location.mapName, err)
            return
        end
        self.mapShown = true
        self:set('map', {text = 'Remove the map'})
        self.results:set('map', 'pass', Location.mapName, string.format('%s placed a map drawn with %s at the %s of the app, centered on %.4f, %.4f.', shown.language, shown.drawnWith, shown.anchor, shown.latitude, shown.longitude))
    end)
end

return Location
