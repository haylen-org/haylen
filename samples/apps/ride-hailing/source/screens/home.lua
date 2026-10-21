-- The map: the day map of the city fills the screen with its traffic and the rider, the round buttons of the menu and of the rider's place float over it, and a sheet asks where to, with the saved places, the recent destinations and the ride in progress.
local assets = require('haylen.assets')
local events = require('haylen.events')
local haylen = require('haylen')
local ui = require('haylen.ui')
local viewport = require('haylen.viewport')

local MapView = require('map-view')
local Page = require('page')
local Places = require('data.places')
local Rides = require('rides')
local Shell = require('shell')
local Style = require('style')

local Home = haylen.class('Home', Page)

Home.recent = {'p12', 'p48', 'p230'}
Home.searchBar = {colors = {raised = Style.color.mist, border = Style.color.mist, text = Style.color.ink}, metrics = {controlRadius = Style.radius.control, iconSize = 30}, fonts = {button = {size = Style.type.heading * 0.85}}}
Home.place = {colors = {raised = Style.color.paper, border = Style.color.line}, metrics = {controlRadius = Style.radius.chip, controlPaddingX = Style.space.m}}

function Home:enter()
    self.map = MapView()
    self.font = assets.font('fonts/barlow-semibold.ttf')
    self:mountGui(ui.stack{id = 'root', onCancel = Shell.back})
    events.on('tripChanged', function()
        self:showCurrent()
    end, {owner = self})
end

function Home:layoutChanged(wide)
    local space = Style.space
    local recent = {}
    for _, id in ipairs(Home.recent) do
        local place = Places.find(id)
        recent[#recent + 1] = {id = id, text = place.name, caption = place.line, image = Shell.icon('clock')}
    end
    local places = {gap = space.xs, wrap = true}
    for _, saved in ipairs(Places.saved) do
        places[#places + 1] = ui.button{id = 'saved-' .. saved.id, text = saved.text, icon = Shell.icon(saved.icon), iconColor = 'text', height = 52, style = Home.place, onClick = function()
            Home.preview(saved.id)
        end}
    end
    -- A phone shows the menu as a round button over the map, and a wide window in the head of the panel.
    self.gui:replaceChildren('root', {
        Shell.mapButton('menu', 'menu', 'Menu', function()
            Shell.open(require('screens.menu'), nil, Shell.menuTransition)
        end, {anchor = 'topLeft', margin = space.m, visible = not wide}),
        Shell.mapButton('locate', 'locate', 'Show where I am', function()
            self.map:reset()
        end, {anchor = 'topRight', margin = space.m}),
        Shell.sheet('sheet', {
            ui.row{id = 'panelHead', visible = wide, gap = space.s, Shell.mapButton('menuWide', 'menu', 'Menu', function()
                Shell.open(require('screens.menu'), nil, Shell.menuTransition)
            end), ui.image{image = 'images/logo.svg', width = 44, height = 44, accessibilityHidden = true}, ui.label{text = 'Ride Hailing', font = 'heading', grow = 1, wrap = false}},
            ui.card{id = 'current', visible = false, padding = {space.s, space.m}, style = {colors = {raised = Style.color.yellow, border = Style.color.yellow}, metrics = {panelRadius = Style.radius.control}},
                ui.row{gap = space.s,
                    ui.icon{image = Shell.icon('car'), size = 34, color = 'text'},
                    ui.column{gap = 0, grow = 1, ui.label{id = 'currentTitle', text = '', font = 'button', wrap = false}, ui.label{id = 'currentCaption', text = '', font = 'caption', wrap = false}},
                    ui.button{id = 'openTrip', text = 'Open', style = {colors = {raised = Style.color.ink, border = Style.color.ink, text = Style.color.paper}}, onClick = function()
                        Shell.open(require('screens.trip'))
                    end},
                },
            },
            ui.label{text = 'Good evening, Rosa', font = 'title', style = Style.text('title', Style.type.heading)},
            ui.button{id = 'whereTo', text = 'Where to?', icon = Shell.icon('search'), iconColor = 'text', justify = 'start', align = 'stretch', height = 72, autofocus = true, style = Home.searchBar, onClick = function()
                Shell.open(require('screens.search'))
            end},
            ui.row(places),
            ui.label{text = 'Recent', font = 'caption', color = 'textMuted', margin = {space.xxs, 0, 0, 0}},
            ui.list{id = 'recent', items = recent, onSelect = function(event)
                Home.preview(event.item)
            end},
        }, wide),
    })
    self:showCurrent()
end

function Home.preview(placeId)
    Shell.open(require('screens.preview'), {to = placeId})
end

function Home:showCurrent()
    local trip = Rides.current
    if not self.gui:has('current') then
        return
    end
    self.gui:set('current', {visible = trip ~= nil})
    if trip then
        self.gui:set('currentTitle', {text = Rides.stages[trip.stage].title})
        self.gui:set('currentCaption', {text = 'To ' .. trip.title})
    end
end

-- Fits the place of the rider into the part of the screen the sheet leaves.
function Home:update(dt)
    Page.update(self, dt)
    local sheet = self.gui:bounds('sheet')
    if not sheet then
        return
    end
    local area = viewport.safeRect()
    if self.wide then
        area = {x = sheet.x + sheet.width, y = area.y, width = area.x + area.width - sheet.x - sheet.width, height = area.height}
    else
        area = {x = area.x, y = area.y, width = area.width, height = sheet.y - area.y}
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
    map:drawPulse(Rides.city:corner(here.corner[1], here.corner[2]))
end

return Home
