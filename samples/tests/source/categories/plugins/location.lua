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
        checksEmpty = 'Find the location or show the map to check them.',
        controls = {
            ui.button{id = 'locate', text = 'Find the location', variant = 'primary', onClick = function() self:locate() end},
            ui.button{id = 'map', text = 'Show the map', onClick = function() self:toggleMap() end},
            ui.label{text = 'Core Location and MapKit on Apple platforms, "LocationManager" and a drawn map on Android, and the Geolocation API and a drawn map on the web. The system asks the person for the location the first time.', color = 'textMuted', font = 'caption'},
        },
        showcase = {
            ui.card{align = 'stretch', gap = 12,
                ui.label{text = 'Where the device is', font = 'heading'},
                ui.grid{columns = 3, gap = 16,
                    Location.value('latitude', 'Latitude'),
                    Location.value('longitude', 'Longitude'),
                    Location.value('accuracy', 'Accuracy'),
                },
                ui.label{id = 'placeState', text = 'Not found yet. The map shows a hill above a bay until then.', font = 'caption', color = 'textMuted'},
            },
            ui.card{grow = 1, align = 'stretch', gap = 12, justify = 'center',
                DemoTest.placeholder('mapState', 'plugins/map.svg', 'The native map shows at the bottom of the app, over it, once you show it.'),
            },
        },
        code = "local place, err = demo.location():await()  -- The system asks the person first.\nprint(place.latitude, place.longitude, place.accuracy)\ndemo.showMap('bottom', place.latitude, place.longitude):await()\ndemo.removeMap()",
    }
end

-- A value of the place with its name over it.
function Location.value(id, name)
    return ui.column{gap = 4,
        ui.label{text = name, font = 'caption', color = 'textMuted'},
        ui.label{id = id, text = '–', font = 'heading'},
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
        self:set('placeState', {text = 'Looking for the device.'})
        local place, err = demo.location():await()
        if err then
            self:set('placeState', {text = 'The platform gave no place: ' .. err.message})
            self.results:failure('location', Location.locationName, err)
            return
        end
        self.place = place
        self:set('latitude', {text = string.format('%.5f°', place.latitude)})
        self:set('longitude', {text = string.format('%.5f°', place.longitude)})
        self:set('accuracy', {text = string.format('%.0f m', place.accuracy)})
        self:set('placeState', {text = string.format('Found by %s. The map shows this place now.', place.language)})
        self.results:set('location', 'pass', Location.locationName, string.format('%s found %.5f, %.5f within %.0f meters.', place.language, place.latitude, place.longitude, place.accuracy))
    end)
end

function Location:toggleMap()
    if self.mapShown then
        self.mapShown = false
        demo.removeMap()
        self:set('map', {text = 'Show the map'})
        self:set('mapState-text', {text = 'The map is removed. Show it again to place it over the app.'})
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
        self:set('mapState-text', {text = string.format('The map drawn with %s shows at the %s of the app, centered on %.4f, %.4f.', shown.drawnWith, shown.anchor, shown.latitude, shown.longitude)})
        self.results:set('map', 'pass', Location.mapName, string.format('%s placed a map drawn with %s at the %s of the app.', shown.language, shown.drawnWith, shown.anchor))
    end)
end

return Location
