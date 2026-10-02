-- Split screen: every camera with a viewport draws into its own part of the screen, so one frame draws the world once per player.
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local ui = require('haylen.ui')
local viewport = require('haylen.viewport')

local Player = require('player')
local World = require('world')
local sample = require('sample')

local SplitScreen = haylen.class('SplitScreen', sample.Test)

SplitScreen.hints = 'Yellow walks with WASD or the left stick, blue with IJKL or the right stick, and each half follows the pointer held inside it.'

function SplitScreen:init(entry)
    SplitScreen.super.init(self, entry)
    self.world = World()
    self.players = {Player(-400, 100, '#FFFFD040', 'move'), Player(400, -100, '#FF40A0FF', 'move2')}
    self.cameras = {}
    for index = 1, 2 do
        local camera = graphics2d.newCamera()
        camera.positionSmoothing = true
        camera.limits = World.bounds
        self.cameras[index] = camera
    end
    self.vertical = true
    self:layout()
end

function SplitScreen:controls()
    return {ui.formField{label = 'Split', ui.radioGroup{selected = 'vertical', horizontal = true, items = {{id = 'vertical', text = 'Side by side'}, {id = 'horizontal', text = 'Stacked'}}, onChange = function(event)
        self.vertical = event.value == 'vertical'
    end}}}
end

-- Gives each camera its half of the visible area, which follows the window when it is resized.
function SplitScreen:layout()
    local area = viewport.visibleRect()
    if self.vertical then
        self.cameras[1].viewport = {area.x, area.y, area.width / 2, area.height}
        self.cameras[2].viewport = {area.x + area.width / 2, area.y, area.width / 2, area.height}
    else
        self.cameras[1].viewport = {area.x, area.y, area.width, area.height / 2}
        self.cameras[2].viewport = {area.x, area.y + area.height / 2, area.width, area.height / 2}
    end
end

function SplitScreen:update(dt)
    self:layout()
    for index, player in ipairs(self.players) do
        player:update(dt, self.cameras[index])
        self.cameras[index]:follow(player.x, player.y, dt)
        self.cameras[index]:update(dt)
    end
    local distance = math.sqrt((self.players[1].x - self.players[2].x) ^ 2 + (self.players[1].y - self.players[2].y) ^ 2)
    self:setStatus(string.format('Players %.0f apart, %d canvases', distance, graphics2d.stats().canvases))
end

function SplitScreen:render()
    for _, camera in ipairs(self.cameras) do
        graphics2d.beginWorld(camera)
        self.world:draw()
        for _, player in ipairs(self.players) do
            player:draw()
        end
    end
end

function SplitScreen:renderUi()
    graphics2d.beginScreen()
    local view = self.cameras[2].viewport
    if self.vertical then
        graphics2d.drawRect({view.x - 4, view.y, 8, view.height}, '#FF101418')
    else
        graphics2d.drawRect({view.x, view.y - 4, view.width, 8}, '#FF101418')
    end
end

return SplitScreen
