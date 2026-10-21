-- Parallax layers: each layer follows the camera by its scroll scale and repeats to fill the view, so far layers drift slowly, the ground moves with the camera, near grass races by and clouds scroll by themselves.
local animation2d = require('haylen.animation2d')
local assets = require('haylen.assets')
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local input = require('haylen.input')
local ui = require('haylen.ui')

local CameraTest = require('categories.camera.camera-test')
local Walker = require('categories.camera.walker')
local World = require('categories.camera.world')
local actions = require('categories.camera.actions')

local Parallax = haylen.class('Parallax', CameraTest)

Parallax.code = [[
local texture = assets.texture('camera/hills.png', {filter = 'linear', wrap = 'repeat'})
local hills = graphics2d.newParallax(texture, {position = {0, -224}, size = {1096, 538}, scrollScale = {0.45, 1}, repeatX = true})
local clouds = graphics2d.newParallax(cloudTexture, {scrollScale = {0.05, 1}, repeatX = true, autoscroll = {-60, 0}})
clouds:update(dt)  -- Advances the autoscroll.
hills:draw(camera, {layer = -2})]]

Parallax.ground = 300
-- The fence of the painted terrain tiles, repeated along the back of the ground.
Parallax.fence = {source = {926, 2, 128, 128}, size = 150}

function Parallax.texture(name)
    return assets.texture('camera/' .. name .. '.png', {filter = 'linear', wrap = 'repeat'})
end

function Parallax:init(entry)
    Parallax.super.init(self, entry)
    local sheet = World.texture('sprites/sheets/walker.png')
    self.walker = {x = 0, sprite = graphics2d.newSprite(sheet, {scaleX = 1, scaleY = 1, layer = 1}), animator = animation2d.newAnimator()}
    self.walker.animator:add('idle', animation2d.fromGrid(sheet, {frameWidth = 256, frameHeight = 256, cells = {1, 2, 3, 4}, framesPerSecond = 6}))
    self.walker.animator:add('run', animation2d.fromGrid(sheet, {frameWidth = 256, frameHeight = 256, cells = {9, 10, 11, 12, 13, 14, 15, 16}, framesPerSecond = 14}))
    self.walker.animator.pivotY = Walker.feet
    self.walker.animator:play('run')
    self.terrain = World.texture('tiled/tiles/terrain.png')
    self.traveling = true
    self.layers = {
        {name = 'Clouds drift at 0.05', y = -400, layer = graphics2d.newParallax(Parallax.texture('clouds'), {position = {0, -580}, size = {1280, 402}, scrollScale = {0.05, 1}, repeatX = true, autoscroll = {-60, 0}, color = '#D0FFFFFF'}), order = -4},
        {name = 'Mountains at 0.15', y = -150, layer = graphics2d.newParallax(Parallax.texture('mountains'), {position = {0, -480}, size = {1280, 572}, scrollScale = {0.15, 1}, repeatX = true}), order = -3},
        {name = 'Hills at 0.45', y = 90, layer = graphics2d.newParallax(Parallax.texture('hills'), {position = {0, Parallax.ground - 524}, size = {1096, 538}, scrollScale = {0.45, 1}, repeatX = true}), order = -2},
        {name = 'Ground at 1', y = 330, layer = graphics2d.newParallax(Parallax.texture('ground'), {position = {0, Parallax.ground - 52}, size = {1076, 256}, scrollScale = {1, 1}, repeatX = true}), order = -1},
        {name = 'Grass at 1.35', y = 520, layer = graphics2d.newParallax(Parallax.texture('grass'), {position = {0, Parallax.ground + 21}, size = {512, 227}, scrollScale = {1.35, 1}, repeatX = true}), order = 5},
    }
end

function Parallax:enter()
    self:loadActions(actions)
    self:frame{
        hint = 'Walk with A and D, the arrows, the left stick or by holding the mouse button or a finger on either side, or let the camera travel by itself.',
        code = Parallax.code,
        play = true,
        view = {1920, 1080},
        controls = {ui.toggle{id = 'travel', text = 'Camera travels by itself', checked = true, onChange = function(event)
            self.traveling = event.checked
        end}},
    }
end

-- Returns the screen x of the mouse button or the finger held on the stage, or nothing.
function Parallax:heldX()
    if ui.usingPointer() then
        return nil
    end
    local touch = input.touches()[1]
    if touch then
        return self.stage:contains({touch.x, touch.y}) and touch.x or nil
    end
    local x, y = input.mousePosition()
    return input.down('point') and self.stage:contains({x, y}) and x or nil
end

function Parallax:update(dt)
    Parallax.super.update(self, dt)
    if not self.stage then
        return
    end
    local direction = input.vector('move')
    local x = self:heldX()
    if x then
        direction = x < self.camera:worldToScreen(self.walker.x, 0) and -1 or 1
    end
    if direction ~= 0 and self.traveling then
        self.traveling = false
        self:set('travel', {checked = false})
    end
    local speed = (self.traveling and 1 or direction) * 520
    local walker = self.walker
    walker.x = walker.x + speed * dt
    self.camera.x = walker.x
    local clip = speed ~= 0 and 'run' or 'idle'
    if walker.animator.current ~= clip then
        walker.animator:play(clip)
    end
    if speed ~= 0 then
        walker.sprite.flipHorizontal = speed < 0
    end
    walker.animator:update(dt)
    for _, entry in ipairs(self.layers) do
        entry.layer:update(dt)
    end
    self:status(speed ~= 0 and 'Every layer slides past at its own share of the speed of the camera.' or 'The camera rests, and only the clouds drift by themselves.')
end

function Parallax:draw(area)
    local view = graphics2d.canvasBounds()
    graphics2d.drawRect(view, '#FF8CC8F0', {layer = -10})
    for _, entry in ipairs(self.layers) do
        entry.layer:draw(self.camera, {layer = entry.order})
    end
    -- The earth of the ground strip goes on below it to the bottom of the view.
    graphics2d.drawRect({view.x, Parallax.ground + 200, view.width, 600}, '#FF81411F', {layer = -1})
    local fence = Parallax.fence
    for x = math.floor(view.x / fence.size) * fence.size, view:right(), fence.size do
        graphics2d.draw(self.terrain, x, Parallax.ground + 16, {source = fence.source, width = fence.size, height = fence.size, pivotX = 0, pivotY = 1, layer = 0})
    end
    local walker = self.walker
    walker.sprite.x, walker.sprite.y = walker.x, Parallax.ground + 40
    walker.animator:apply(walker.sprite)
    walker.sprite:draw()
    for _, entry in ipairs(self.layers) do
        CameraTest.tag(entry.name, view.x + 24 * graphics2d.canvasUnitSize(), entry.y, {anchorX = 0})
    end
end

return Parallax
