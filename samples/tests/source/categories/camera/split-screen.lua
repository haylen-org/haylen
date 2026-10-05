-- Split screen: every camera with a viewport draws into its own part of the screen, so one frame draws the world once per walker.
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local ui = require('haylen.ui')

local Test = require('harness.test')
local Walker = require('categories.camera.walker')
local World = require('categories.camera.world')
local actions = require('categories.camera.actions')

local SplitScreen = haylen.class('SplitScreen', Test)

function SplitScreen:init(entry)
    SplitScreen.super.init(self, entry)
    self.world = World()
    self.walkers = {Walker(-400, 100, '#FFFFD040', 'move'), Walker(400, -100, '#FF40A0FF', 'move2')}
    self.cameras = {}
    for index = 1, 2 do
        local camera = graphics2d.newCamera()
        camera.positionSmoothing = true
        camera.limits = World.bounds
        self.cameras[index] = camera
    end
    self.vertical = true
end

function SplitScreen:enter()
    self:loadActions(actions)
    self:frame{
        hint = 'Yellow walks with WASD, the arrows or the left stick, blue with IJKL or the right stick, and each half follows the pointer held inside it.',
        play = true,
        controls = {ui.formField{label = 'Split', ui.radioGroup{id = 'split', selected = 'vertical', items = {{id = 'vertical', text = 'Side by side'}, {id = 'horizontal', text = 'Stacked'}}, onChange = function(event)
            self.vertical = event.value == 'vertical'
        end}}},
    }
end

-- Gives each camera its half of the play area, which follows the layout when the window is resized.
function SplitScreen:layout(stage)
    if self.vertical then
        self.cameras[1].viewport = {stage.x, stage.y, stage.width / 2, stage.height}
        self.cameras[2].viewport = {stage.x + stage.width / 2, stage.y, stage.width / 2, stage.height}
    else
        self.cameras[1].viewport = {stage.x, stage.y, stage.width, stage.height / 2}
        self.cameras[2].viewport = {stage.x, stage.y + stage.height / 2, stage.width, stage.height / 2}
    end
end

function SplitScreen:update(dt)
    SplitScreen.super.update(self, dt)
    if not self.stage then
        return
    end
    self:layout(self.stage)
    for index, walker in ipairs(self.walkers) do
        walker:update(dt, self.cameras[index])
        self.cameras[index]:follow(walker.x, walker.y, dt)
        self.cameras[index]:update(dt)
    end
    local distance = math.sqrt((self.walkers[1].x - self.walkers[2].x) ^ 2 + (self.walkers[1].y - self.walkers[2].y) ^ 2)
    self:status(string.format('Walkers %.0f apart   Canvases %d', distance, graphics2d.stats().canvases))
end

function SplitScreen:render()
    if not self.stage then
        return
    end
    for _, camera in ipairs(self.cameras) do
        graphics2d.beginWorld(camera)
        self.world:draw()
        for _, walker in ipairs(self.walkers) do
            walker:draw()
        end
    end
end

function SplitScreen:renderUi()
    if not self.stage then
        return
    end
    graphics2d.beginScreen()
    local view = self.cameras[2].viewport
    if self.vertical then
        graphics2d.drawRect({view.x - 4, view.y, 8, view.height}, '#FF101418')
    else
        graphics2d.drawRect({view.x, view.y - 4, view.width, 8}, '#FF101418')
    end
end

return SplitScreen
