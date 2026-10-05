-- Parallax layers: each layer follows the camera by its scroll scale and repeats to fill the view, so far layers drift slowly, near ones race by and clouds scroll by themselves.
local assets = require('haylen.assets')
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local input = require('haylen.input')
local ui = require('haylen.ui')

local Test = require('harness.test')
local actions = require('categories.camera.actions')

local Parallax = haylen.class('Parallax', Test)

Parallax.ground = 300

function Parallax.texture(name)
    return assets.texture('camera/' .. name .. '.png', {filter = 'linear', wrap = 'repeat'})
end

function Parallax:init(entry)
    Parallax.super.init(self, entry)
    self.walker = {x = 0}
    self.traveling = true
    self.layers = {
        {name = 'Clouds 0.05 and autoscroll', layer = graphics2d.newParallax(Parallax.texture('clouds'), {position = {0, -540}, size = {1024, 320}, scrollScale = {0.05, 1}, repeatX = true, autoscroll = {-60, 0}, color = '#D0FFFFFF'}), order = -4},
        {name = 'Mountains 0.15', layer = graphics2d.newParallax(Parallax.texture('mountains'), {position = {0, -420}, size = {1024, 512}, scrollScale = {0.15, 1}, repeatX = true}), order = -3},
        {name = 'Hills 0.45', layer = graphics2d.newParallax(Parallax.texture('hills'), {position = {0, Parallax.ground - 384}, size = {1024, 384}, scrollScale = {0.45, 1}, repeatX = true}), order = -2},
        {name = 'Grass 1.35', layer = graphics2d.newParallax(Parallax.texture('grass'), {position = {0, Parallax.ground + 120}, size = {512, 128}, scrollScale = {1.35, 1}, repeatX = true}), order = 5},
    }
end

function Parallax:enter()
    self:loadActions(actions)
    self:frame{
        hint = 'Walk with A and D, the arrows, the left stick or by holding the mouse button or a finger on either side, or let the camera travel by itself.',
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
    self.walker.x = self.walker.x + (self.traveling and 1 or direction) * 520 * dt
    self.camera.x = self.walker.x
    for _, entry in ipairs(self.layers) do
        entry.layer:update(dt)
    end
    self:status(string.format('Camera x %.0f   Clouds scrolled %.0f', self.camera.x, self.layers[1].layer:scrolled().x))
end

function Parallax:draw(area)
    local view = graphics2d.canvasBounds()
    graphics2d.drawRect(view, '#FF8CC8F0', {layer = -10})
    for _, entry in ipairs(self.layers) do
        entry.layer:draw(self.camera, {layer = entry.order})
    end
    graphics2d.drawRect({view.x, Parallax.ground, view.width, 600}, '#FF5A8A3A')
    graphics2d.drawCircle(self.walker.x, Parallax.ground - 30, 30, '#FFFFD040', {layer = 1})
    for post = math.floor(view.x / 400) * 400, view:right(), 400 do
        graphics2d.drawRect({post - 8, Parallax.ground - 80, 16, 80}, '#FF6A4A2A')
    end
end

function Parallax:renderUi()
    local stage = self.stage
    if not stage then
        return
    end
    graphics2d.beginScreen()
    for index, entry in ipairs(self.layers) do
        graphics2d.drawText(nil, entry.name, stage.x + 24, stage:bottom() - 24 - index * 36, {size = 28, outlineWidth = 3})
    end
end

return Parallax
