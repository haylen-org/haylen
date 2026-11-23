-- The only screen: it drives the car, lays the dashboard out for the screen, draws the backdrop, the cards and the cluster, and hosts the controls along the bottom.
local assets = require('haylen.assets')
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local scene = require('haylen.scene')
local viewport = require('haylen.viewport')
local window = require('haylen.window')

local Cluster = require('cluster')
local Controls = require('controls')
local Drive = require('drive')
local Layout = require('layout')
local Panels = require('panels')
local Style = require('style')

local Cockpit = haylen.class('Cockpit', scene.Scene)

-- The design sizes of the two orientations: a landscape screen sees a wide dashboard, and a portrait one a tall column with the same sizes of text.
Cockpit.landscapeSize = {1920, 1080}
Cockpit.portraitSize = {1080, 1920}

Cockpit.icons = {'weather', 'thermometer', 'wifi', 'bluetooth', 'flag', 'bolt', 'tyre', 'leaf', 'traction', 'lane', 'cruise'}

function Cockpit:enter()
    Style.load()
    Cluster.prepare()
    Panels.prepare()
    local images = {
        rear = assets.texture('images/car-rear.png', {filter = 'linear', mipmaps = true}),
        top = assets.texture('images/car-top.png', {filter = 'linear', mipmaps = true}),
    }
    local icons = {}
    for _, name in ipairs(Cockpit.icons) do
        icons[name] = assets.vectorImage('icons/' .. name .. '.svg')
    end
    self.icons = icons
    self.drive = Drive.new(7)
    self.layout = Layout.new()
    self.cluster = Cluster.new(images)
    self.panels = Panels.new(images, icons)
    self.controls = Controls.new(self)
    self.visible = {pivotX = 0, pivotY = 0, x = 0, y = 0, width = 0, height = 0}

    -- The layout follows the window and the safe area for a few frames after they change, while the bar of the controls lays out in its new place, and costs nothing in the other frames.
    self.layoutFrames = Cockpit.layoutFrames
    for _, name in ipairs({'windowResized', 'windowSafeAreaChanged'}) do
        self:listen(name, function()
            self.layoutFrames = Cockpit.layoutFrames
        end)
    end
end

Cockpit.layoutFrames = 3

-- Picks the design size of the orientation of the window, so the dashboard keeps its sizes when the screen turns.
function Cockpit:orient()
    local pixels = viewport.pixelRect()
    local landscape = pixels.width >= pixels.height
    local size = landscape and Cockpit.landscapeSize or Cockpit.portraitSize
    local width, height = viewport.designSize()
    if width ~= size[1] or height ~= size[2] then
        viewport.setDesignSize(size[1], size[2])
    end
    self.controls:build(landscape)
end

-- The car drives all the time, so the dashboard asks for every frame.
function Cockpit:update(dt)
    window.requestRedraw()
    if self.layoutFrames > 0 then
        self.layoutFrames = self.layoutFrames - 1
        self:layOut()
    end
    self.drive:update(dt)
    self.controls:update(dt)
end

function Cockpit:layOut()
    self:orient()
    local safe = viewport.safeRect()
    if self.layout:update(safe, self.controls:top(safe.y + safe.height - 150)) then
        self.cluster:layOut(self.layout.gauge)
    end
    local visible = viewport.visibleRect()
    local backdrop = self.visible
    backdrop.x, backdrop.y, backdrop.width, backdrop.height = visible.x, visible.y, visible.width, visible.height
end

function Cockpit:render()
    graphics2d.beginScreen()
    local backdrop = self.visible
    graphics2d.draw(Style.backdrop, backdrop.x, backdrop.y, backdrop)

    local layout, drive, panels, cluster = self.layout, self.drive, self.panels, self.cluster
    panels:drawStatus(layout.status, drive, self.controls)
    panels:drawNavigation(layout.navigation, drive)
    panels:drawTrip(layout.trip, drive)
    panels:drawBattery(layout.battery, drive)
    panels:drawConsumption(layout.consumption, drive)
    panels:drawTyres(layout.tyres, drive)
    panels:drawAssistants(layout.assistants)
    cluster:drawGauge(layout.gauge, drive)
    cluster:drawRoad(layout.gauge, layout.road, drive)
    cluster:drawStrip(layout.strip, drive, self.icons)
end

return Cockpit
