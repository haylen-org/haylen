-- The rule "delivery/later/**" of "app.json" delivers the maps of the folder on demand: they reach the device only when the app asks for them. A synchronous load never waits for a download, so it refuses a map that is not on the device, while "assets.loadAsync" brings the map first and then loads it. In development the folder holds every file and a request takes one frame, so a mistake shows long before a release.
local assets = require('haylen.assets')
local content = require('haylen.content')
local haylen = require('haylen')
local ui = require('haylen.ui')

local Test = require('harness.test')
local mapView = require('categories.delivery.map-view')

local OnDemand = haylen.class('OnDemand', Test)

OnDemand.paths = {'delivery/later/island.json', 'delivery/later/reef.json'}

function OnDemand:enter()
    local items = {}
    for index, path in ipairs(OnDemand.paths) do
        items[index] = {id = path, text = path}
    end
    self:frame{
        code = "local map, err = assets.loadAsync(path):await()",
        hint = 'Pick a map to load it the synchronous way and then the asynchronous way.',
        focus = 'maps',
        controls = {
            ui.sectionTitle{text = 'Maps delivered on demand'},
            ui.list{id = 'maps', items = items, onSelect = function(event)
                self:open(event.item)
            end},
            ui.label{id = 'local', text = ''},
            ui.label{id = 'synchronous', text = '', color = 'textMuted'},
            ui.label{id = 'asynchronous', text = ''},
        },
    }
    self:open(OnDemand.paths[1])
end

-- Tries the synchronous load, which raises for a map that is not on the device, and then loads the map asynchronously.
function OnDemand:open(path)
    local wasLocal = content.isLocal(path)
    self:set('local', {text = string.format('Before the load, "%s" is on the device: %s.', path, tostring(wasLocal))})
    local ok, failure = pcall(assets.json, path)
    self:set('synchronous', {text = ok and 'The synchronous load read the map, which an earlier load brought.' or ('The synchronous load raised: ' .. tostring(failure))})
    self:set('asynchronous', {text = 'Loading asynchronously.', color = 'textMuted'})
    self:spawn(function()
        local map, err = assets.loadAsync(path):await()
        if not map then
            self:set('asynchronous', {text = 'The asynchronous load failed: ' .. tostring(err), color = 'dangerText'})
            return
        end
        self.map = map
        self:set('asynchronous', {text = string.format('The asynchronous load brought "%s", which is now on the device: %s.', map.name, tostring(content.isLocal(path))), color = 'successText'})
        self:log('Loaded %s on demand.', path)
    end)
end

function OnDemand:draw(area)
    mapView.draw(self.map, area)
    Test.caption(self.map and 'The map that came on demand.' or 'Waiting for the map.', 16, 16)
end

return OnDemand
