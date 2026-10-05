-- Shake: trauma decays over time and the shake grows with its square, so small hits stay subtle, and `camera:shake` moves the view only along one direction, like a recoil.
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local input = require('haylen.input')
local ui = require('haylen.ui')

local Test = require('harness.test')
local Walker = require('categories.camera.walker')
local World = require('categories.camera.world')
local actions = require('categories.camera.actions')

local Shake = haylen.class('Shake', Test)

function Shake:init(entry)
    Shake.super.init(self, entry)
    self.world = World()
    self.walker = Walker(0, 0, '#FFFFD040', 'move')
    self.camera = graphics2d.newCamera()
    self.camera.maxShakeOffset = 40
    self.camera.maxShakeAngle = 0.06
end

function Shake:enter()
    local camera = self.camera
    self:loadActions(actions)
    self:frame{
        hint = 'A click or a tap on the play area, F or the X button adds trauma too. Walk with WASD, the arrows or the left stick to feel the shake while moving.',
        play = true,
        controls = {
            ui.row{gap = 8,
                ui.button{id = 'hit', text = 'Hit', grow = 1, onClick = function() camera:addTrauma(0.35) end},
                ui.button{id = 'blast', text = 'Blast', grow = 1, onClick = function() camera:addTrauma(0.9) end},
                ui.button{id = 'recoil', text = 'Recoil', grow = 1, onClick = function() camera:shake(0.7, 1, 0) end},
            },
            ui.formField{label = 'Frequency', ui.slider{id = 'frequency', min = 2, max = 60, value = camera.shakeFrequency, showValue = true, decimals = 0, onChange = function(event)
                camera.shakeFrequency = event.value
            end}},
            ui.formField{label = 'Largest offset', ui.slider{id = 'offset', min = 0, max = 120, value = camera.maxShakeOffset, showValue = true, decimals = 0, onChange = function(event)
                camera.maxShakeOffset = event.value
            end}},
            ui.formField{label = 'Trauma lost per second', ui.slider{id = 'decay', min = 0.2, max = 4, value = camera.traumaDecay, showValue = true, onChange = function(event)
                camera.traumaDecay = event.value
            end}},
        },
    }
end

-- Tells whether the place action fired this frame, or the mouse button or a finger went down on the stage.
function Shake:pressed()
    if input.pressed('place') then
        return true
    end
    if ui.usingPointer() then
        return false
    end
    local touch = input.touches()[1]
    if touch then
        return touch.phase == 'began' and self.stage:contains({touch.x, touch.y})
    end
    return input.mousePressed('left') and self.stage:contains({input.mousePosition()})
end

function Shake:update(dt)
    Shake.super.update(self, dt)
    if self.stage and self:pressed() then
        self.camera:addTrauma(0.5)
    end
    self.walker:update(dt)
    self.camera:follow(self.walker.x, self.walker.y, dt)
    self.camera:update(dt)
    local offset = self.camera:shakeOffset()
    self:status(string.format('Trauma %.2f   Offset %.1f, %.1f   Angle %.3f', self.camera.trauma, offset.x, offset.y, self.camera:renderRotation()))
end

function Shake:draw(area)
    self.world:draw()
    self.walker:draw()
end

-- The trauma bar stays still in a corner of the play area while the view shakes.
function Shake:renderUi()
    local stage = self.stage
    if not stage then
        return
    end
    graphics2d.beginScreen()
    local x, y = stage.x + 24, stage:bottom() - 48
    graphics2d.drawRect({x, y, 400, 24}, '#80000000')
    graphics2d.drawRect({x, y, 400 * self.camera.trauma, 24}, '#FFFF6040', {layer = 1})
end

return Shake
