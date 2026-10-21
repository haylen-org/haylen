-- The rule "delivery/later/**" of "app.json" delivers the maps of the folder on demand: they reach the device only when the app asks for them. A synchronous load never waits for a download, so it refuses a map that is not on the device, while "assets.loadAsync" brings the map first and then loads it. In development the folder holds every file and a request takes one frame, so a mistake shows long before a release.
local assets = require('haylen.assets')
local content = require('haylen.content')
local haylen = require('haylen')
local ui = require('haylen.ui')

local Test = require('harness.test')
local MapView = require('categories.delivery.map-view')

local OnDemand = haylen.class('OnDemand', Test)

OnDemand.maps = {
    {path = 'delivery/later/island.json', name = 'Island'},
    {path = 'delivery/later/reef.json', name = 'Reef'},
}
OnDemand.icons = {cloud = 'delivery/icons/cloud.svg', device = 'delivery/icons/device.svg'}

function OnDemand:enter()
    self.mapView = MapView()
    self:frame{
        code = [[
local ok, failure = pcall(assets.json, path)  -- Refuses a map that is not on the device.
local map, err = assets.loadAsync(path):await()  -- Brings the map first.
print(content.isLocal(path))]],
        hint = 'Pick a map to load it the synchronous way and then the asynchronous way. A cloud marks a map that is not on the device yet.',
        focus = 'maps',
        controls = {
            ui.sectionTitle{text = 'Maps delivered on demand'},
            ui.list{id = 'maps', items = self:items(), onSelect = function(event)
                self:open(event.item)
            end},
            ui.sectionTitle{text = 'The synchronous load'},
            ui.alert{id = 'synchronous', tone = 'information', title = '', message = ''},
            ui.sectionTitle{text = 'The asynchronous load'},
            ui.row{gap = 12, alignItems = 'center',
                ui.badge{id = 'asynchronous', text = 'Waiting', tone = 'neutral'},
                ui.label{id = 'local', text = '', color = 'textMuted', grow = 1},
            },
        },
    }
    self:open(OnDemand.maps[1].path)
end

-- The maps of the list, each with the picture of where it is: on the device or still in the cloud.
function OnDemand:items()
    local items = {}
    for index, map in ipairs(OnDemand.maps) do
        local here = content.isLocal(map.path)
        items[index] = {id = map.path, text = map.name, caption = here and 'On the device' or 'Comes when asked for', image = here and OnDemand.icons.device or OnDemand.icons.cloud, iconColor = false}
    end
    return items
end

-- Tries the synchronous load, which raises for a map that is not on the device, and then loads the map asynchronously.
function OnDemand:open(path)
    self:set('maps', {selected = path})
    local ok, failure = pcall(assets.json, path)
    if ok then
        self:set('synchronous', {tone = 'success', title = 'Read at once', message = 'An earlier load brought the map, so the synchronous load read it.'})
    else
        self:set('synchronous', {tone = 'danger', title = 'Refused', message = tostring(failure)})
    end
    self:set('asynchronous', {text = 'Loading', tone = 'warning'})
    self:spawn(function()
        local map, err = assets.loadAsync(path):await()
        if not map then
            self:set('asynchronous', {text = 'Failed', tone = 'danger'})
            self:set('local', {text = tostring(err)})
            return
        end
        self.map = map
        self:set('asynchronous', {text = 'Loaded', tone = 'success', solid = true})
        self:set('local', {text = string.format('The map "%s" is on the device now.', map.name)})
        self:set('maps', {items = self:items(), selected = path})
        self:log('Loaded %s on demand.', path)
    end)
end

function OnDemand:draw(area)
    self.mapView:draw(self.map, area)
    Test.caption(self.map and string.format('The %s, which came on demand.', self.map.name:lower()) or 'Waiting for the map.', area.width / 2, 20, {size = 22, color = Test.ink, anchor = {0.5, 0}})
end

return OnDemand
