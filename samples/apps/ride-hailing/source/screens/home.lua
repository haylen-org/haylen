-- The ride tab: the night map of the city with its traffic and the rider, under a sheet that asks where to, with the saved places, the recent destinations and the ride in progress.
local assets = require('haylen.assets')
local events = require('haylen.events')
local haylen = require('haylen')
local ui = require('haylen.ui')

local MapView = require('map-view')
local Page = require('page')
local Places = require('data.places')
local Rides = require('rides')
local Shell = require('shell')

local Home = haylen.class('Home', Page)

Home.recent = {'p12', 'p48', 'p230'}

function Home:enter()
    self.map = MapView()
    self.font = assets.font('fonts/barlow-semibold.ttf')
    local recent = {}
    for _, id in ipairs(Home.recent) do
        local place = Places.find(id)
        recent[#recent + 1] = {id = id, text = place.name, caption = place.line, image = Shell.icon('clock')}
    end
    local shortcuts = {gap = 10}
    for _, saved in ipairs(Places.saved) do
        shortcuts[#shortcuts + 1] = ui.button{id = 'saved-' .. saved.id, text = saved.text, icon = Shell.icon(saved.icon), iconColor = 'text', grow = 1, onClick = function()
            Home.preview(saved.id)
        end}
    end
    self:mountGui(Shell.frame('home', ui.column{id = 'content', grow = 1, gap = 0, padding = 14, justify = 'end',
        ui.card{id = 'sheet', gap = 12, padding = {18, 18, 18, 18},
            ui.card{id = 'current', visible = false, padding = {10, 14}, style = {colors = {raised = 'accentBackground'}},
                ui.row{gap = 12,
                    ui.icon{image = Shell.icon('car'), size = 32, color = 'accentText'},
                    ui.column{gap = 0, grow = 1, ui.label{id = 'currentTitle', text = '', font = 'button', wrap = false}, ui.label{id = 'currentCaption', text = '', font = 'caption', color = 'textMuted', wrap = false}},
                    ui.button{id = 'openTrip', text = 'Open', variant = 'primary', onClick = function()
                        Shell.open(require('screens.trip'))
                    end},
                },
            },
            ui.label{text = 'Good evening, Rosa', font = 'heading'},
            ui.button{id = 'whereTo', text = 'Where to?', icon = Shell.icon('search'), iconColor = 'textMuted', justify = 'start', align = 'stretch', autofocus = true, onClick = function()
                Shell.open(require('screens.search'))
            end},
            ui.row(shortcuts),
            ui.label{text = 'Recent', font = 'caption', color = 'textMuted'},
            ui.list{id = 'recent', items = recent, onSelect = function(event)
                Home.preview(event.item)
            end},
        },
    }))
    events.on('tripChanged', function()
        self:showCurrent()
    end, {owner = self})
    self:showCurrent()
end

function Home.preview(placeId)
    Shell.open(require('screens.preview'), {to = placeId})
end

-- The sheet sits at the bottom of a phone and at the top left of a wide window.
function Home:layoutChanged(wide)
    self.gui:set('content', {justify = wide and 'start' or 'end'})
    self.gui:set('sheet', {width = wide and 460 or 'auto', align = wide and 'start' or 'stretch'})
end

function Home:showCurrent()
    local trip = Rides.current
    self.gui:set('current', {visible = trip ~= nil})
    if trip then
        self.gui:set('currentTitle', {text = Rides.stages[trip.stage].title})
        self.gui:set('currentCaption', {text = 'To ' .. trip.title})
    end
end

function Home:update(dt)
    Page.update(self, dt)
    local content, sheet = self.gui:bounds('content'), self.gui:bounds('sheet')
    if not content or not sheet then
        return
    end
    local area = {x = content.x, y = content.y, width = content.width, height = content.height}
    if self.wide then
        area.x = sheet.x + sheet.width
        area.width = content.x + content.width - area.x
    else
        area.height = sheet.y - content.y
    end
    local here = Places.here
    local x, y = Rides.city:corner(here.corner[1], here.corner[2])
    self.map:fit(MapView.bounds({x, y}, 1300), area, dt, not self.placed)
    self.placed = true
end

function Home:render()
    Page.render(self)
    local map = self.map
    map:begin()
    map:drawTraffic()
    local here = Places.here
    local x, y = Rides.city:corner(here.corner[1], here.corner[2])
    map:drawPulse(x, y, ui.themeColor('accent'))
end

return Home
