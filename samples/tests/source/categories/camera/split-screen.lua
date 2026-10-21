-- Split screen: every camera with a viewport draws into its own part of the screen, so one frame draws the world once per walker.
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local ui = require('haylen.ui')

local CameraTest = require('categories.camera.camera-test')
local Walker = require('categories.camera.walker')
local World = require('categories.camera.world')
local actions = require('categories.camera.actions')

local SplitScreen = haylen.class('SplitScreen', CameraTest)

SplitScreen.code = [[
left.viewport = {area.x, area.y, area.width / 2, area.height}
right.viewport = {area.x + area.width / 2, area.y, area.width / 2, area.height}
for _, camera in ipairs({left, right}) do
    graphics2d.beginWorld(camera)  -- Each camera draws the world into its half.
    drawWorld()
end]]

-- Each player starts somewhere else and walks a route of its own until someone steers it.
SplitScreen.players = {
    {name = 'Player 1', color = '#FFFFD040', x = -1000, y = 60, action = 'move', route = {{-400, 230, 1}, {-1000, 60}, {-1600, 320, 1}, {-1000, 60}}},
    {name = 'Player 2', color = '#FF40A0FF', x = 600, y = 590, action = 'move2', route = {{1300, 570, 1}, {1500, 0, 1}, {1100, 620}, {400, 120, 1}, {600, 590}}},
}

function SplitScreen:init(entry)
    SplitScreen.super.init(self, entry)
    self.world = World()
    self.walkers, self.cameras = {}, {}
    for index, player in ipairs(SplitScreen.players) do
        self.walkers[index] = Walker(player.x, player.y, {action = player.action, route = player.route, color = player.color})
        local camera = graphics2d.newCamera()
        camera.positionSmoothing = true
        camera.limits = World.bounds
        camera:snapTo(player.x, player.y)
        self.cameras[index] = camera
    end
    self.vertical = true
end

function SplitScreen:enter()
    self:loadActions(actions)
    self:frame{
        hint = 'Player 1 walks with WASD, the arrows or the left stick, player 2 with IJKL or the right stick, and each half follows the pointer held inside it.',
        code = SplitScreen.code,
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
        walker:update(dt, self.world, self.cameras[index])
        self.cameras[index]:follow(walker.x, walker.y, dt)
        self.cameras[index]:update(dt)
    end
    local distance = math.sqrt((self.walkers[1].x - self.walkers[2].x) ^ 2 + (self.walkers[1].y - self.walkers[2].y) ^ 2)
    self:status(string.format('The players are %d units apart, and the frame draws %d canvases.', math.floor(distance + 0.5), graphics2d.stats().canvases))
end

function SplitScreen:render()
    if not self.stage then
        return
    end
    for _, camera in ipairs(self.cameras) do
        graphics2d.beginWorld(camera, {sort = 'y', visibilityMask = World.view})
        self.world:draw()
        for _, walker in ipairs(self.walkers) do
            walker:draw()
        end
    end
end

-- The divider between the halves and the name of the player of each half stay still while the views move.
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
    for index, camera in ipairs(self.cameras) do
        local half, player = camera.viewport, SplitScreen.players[index]
        CameraTest.tag(player.name, half.x + 100, half.y + 64, {color = player.color})
    end
end

return SplitScreen
