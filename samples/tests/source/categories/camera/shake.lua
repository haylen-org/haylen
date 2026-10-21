-- Shake: trauma decays over time and the shake grows with its square, so small hits stay subtle, and `camera:shake` moves the view only along one direction, like a recoil.
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local input = require('haylen.input')
local ui = require('haylen.ui')

local CameraTest = require('categories.camera.camera-test')
local Walker = require('categories.camera.walker')
local World = require('categories.camera.world')
local actions = require('categories.camera.actions')

local Shake = haylen.class('Shake', CameraTest)

Shake.code = [[
camera.maxShakeOffset = 40
camera.shakeFrequency = 25
camera:addTrauma(0.35)  -- A hit, whose shake grows with the square of the trauma.
camera:shake(0.7, 1, 0)  -- A recoil along x.
camera:update(dt)]]

-- Until a player steers, the walker stops by the shrine, and a hit lands every few seconds.
Shake.route = {{620, 560, 2}, {1050, 600, 2}}
Shake.beat = 2.5

function Shake:init(entry)
    Shake.super.init(self, entry)
    self.world = World()
    self.walker = Walker(620, 560, {action = 'move', route = Shake.route})
    self.camera = graphics2d.newCamera()
    self.camera.maxShakeOffset = 40
    self.camera.maxShakeAngle = 0.06
    self.camera:snapTo(self.walker.x, self.walker.y - 120)
    self.time = Shake.beat - 0.6
    self.shown = -1
end

function Shake:enter()
    local camera = self.camera
    self:loadActions(actions)
    self:frame{
        hint = 'A click or a tap on the play area, F or the X button adds trauma too. Walk with WASD, the arrows or the left stick to feel the shake while moving.',
        code = Shake.code,
        play = true,
        controls = {
            ui.formField{label = 'Trauma', ui.progress{id = 'trauma', value = 0, tone = 'warning'}},
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
            ui.formField{label = 'Trauma lost per second', ui.slider{id = 'decay', min = 0.2, max = 4, value = camera.traumaDecay, showValue = true, decimals = 1, onChange = function(event)
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
    local camera = self.camera
    if self.stage and self:pressed() then
        camera:addTrauma(0.5)
    end
    if self.walker.route then
        self.time = self.time + dt
        if self.time >= Shake.beat then
            self.time = 0
            camera:addTrauma(0.55)
        end
    end
    self.walker:update(dt, self.world)
    camera:follow(self.walker.x, self.walker.y - 120, dt)
    camera:update(dt)

    local trauma = camera.trauma
    if math.abs(trauma - self.shown) > 0.005 then
        self.shown = trauma
        self:set('trauma', {value = trauma, text = string.format('%d percent', math.floor(trauma * 100 + 0.5))})
    end
    local offset = camera:shakeOffset()
    if trauma > 0 then
        self:status(string.format('The view shakes by %d units at %d percent of the trauma.', math.floor(offset:length() + 0.5), math.floor(trauma * 100 + 0.5)))
    else
        self:status('The view rests until the next hit adds trauma.')
    end
end

function Shake:draw(area)
    self.world:draw()
    self.walker:draw()
end

return Shake
