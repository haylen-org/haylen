-- Moving emitters: an emitter measures how fast it moves between updates, so the exhaust of a spaceship takes the share `inheritVelocity` of that velocity, a trail with `rateOverDistance` spawns along the way it moved and leaves no gaps where a trail with only a `rate` does, and its `scale` and `rotation` resize and turn what it spawns from then on. The ship flies a figure eight on its autopilot until the cursor moves.
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local m = require('haylen.math')
local particles2d = require('haylen.particles2d')
local ui = require('haylen.ui')

local ParticleTest = require('categories.particles.particle-test')

local MovingEmitters = haylen.class('MovingEmitters', ParticleTest)

MovingEmitters.code = [[
local exhaust = particles2d.newEmitter({texture = flame, rate = 140, direction = math.pi})
exhaust:configure({speed = {260, 340}, inheritVelocity = 0.8})  -- Keeps the motion of the ship.
local trail = particles2d.newEmitter({texture = dot, rate = 0, rateOverDistance = 0.25})
exhaust.x, exhaust.y = nozzleX, nozzleY  -- The emitter measures its own velocity.
exhaust.scale, exhaust.rotation = 1.5, heading  -- Applies to what it spawns from now on.]]

MovingEmitters.scale = 0.45
-- The two nozzles at the back of the ship, from the middle of its picture of 424 by 332 pixels.
MovingEmitters.nozzles = {{-182, -45}, {-182, 56}}
MovingEmitters.legend = {
    {color = '#FFFFB040', text = 'Orange trail, 60 particles per second with "rate"'},
    {color = '#FF60E0FF', text = 'Blue trail, one particle every 4 units with "rateOverDistance"'},
}

function MovingEmitters:init(entry)
    MovingEmitters.super.init(self, entry)
    self.inherit = 0.8
    self.scale = 1
    self.rotation = 0
    self.autopilot = true
    self.touring = true
    self.time = 0
    self.shipX, self.shipY = 0, 0
    self.heading = 0
    self.speed = 0
    local dot = 'soft-dot'

    self.exhaust = particles2d.newEmitter(ParticleTest.look('flame-soft', {rate = 140, lifetime = {0.4, 0.7}, speed = {260, 340}, direction = math.pi, spread = 0.2, inheritVelocity = self.inherit, startSize = {40, 52}, endSize = 6, colors = {'#C0FFF0C0', '#A0FFB040', '#80FF5020', '#00602010'}, blend = 'additive', maxParticles = 256, layer = 3, seed = 271}))
    self.plain = particles2d.newEmitter(ParticleTest.look(dot, {rate = 60, lifetime = 1.2, speed = 0, startSize = 24, endSize = 4, colors = {'#E0FFB040', '#00FF6020'}, blend = 'additive', maxParticles = 256, layer = 2, seed = 272}))
    self.spaced = particles2d.newEmitter(ParticleTest.look(dot, {rate = 0, rateOverDistance = 0.25, lifetime = 1.2, speed = 0, startSize = 24, endSize = 4, colors = {'#E060E0FF', '#002080FF'}, blend = 'additive', maxParticles = 2048, layer = 2, seed = 273}))
    self.emitters = {self.exhaust, self.plain, self.spaced}
end

function MovingEmitters:enter()
    self:frame{
        hint = 'The ship flies on its autopilot until the cursor moves, then follows it: move it fast with the mouse, a finger, the arrows, WASD or a stick. The orange trail spawns by time and breaks up at speed, and the blue one spawns by distance.',
        code = MovingEmitters.code,
        cursor = true,
        controls = {
            ui.formField{label = 'Velocity the exhaust inherits', ui.slider{id = 'inherit', min = -1, max = 1.5, value = self.inherit, showValue = true, onChange = function(event)
                self.inherit = event.value
                self.exhaust:configure({inheritVelocity = event.value})
            end}},
            ui.formField{label = 'Scale of the exhaust', ui.slider{id = 'scale', min = 0.25, max = 2.5, value = self.scale, showValue = true, onChange = function(event)
                self.scale = event.value
                self.exhaust.scale = event.value
            end}},
            ui.formField{label = 'Turn of the exhaust', ui.slider{id = 'rotation', min = -3.1416, max = 3.1416, value = self.rotation, showValue = true, onChange = function(event)
                self.rotation = event.value
            end}},
            ui.toggle{id = 'autopilot', text = 'Autopilot on a figure eight', checked = self.autopilot, onChange = function(event)
                self.autopilot = event.checked
                self.touring = false
            end},
        },
    }
end

-- Returns a point of the ship, given in pixels of its picture from its middle, in the world. A ship that flies to the left is mirrored, so it never turns upside down.
function MovingEmitters:place(point)
    local mirror = math.cos(self.heading) < 0 and -1 or 1
    local x, y = point[1] * MovingEmitters.scale, point[2] * MovingEmitters.scale * mirror
    local cos, sin = math.cos(self.heading), math.sin(self.heading)
    return self.shipX + x * cos - y * sin, self.shipY + x * sin + y * cos
end

function MovingEmitters:update(dt)
    MovingEmitters.super.update(self, dt)
    if self.touring and (math.abs(self.cursorX) > 1 or math.abs(self.cursorY) > 1) then
        self.touring = false
        self.autopilot = false
        self:set('autopilot', {checked = false})
    end
    local x, y = self.cursorX, self.cursorY
    if self.autopilot then
        self.time = self.time + dt
        x, y = math.sin(self.time * 1.6) * 640, math.sin(self.time * 3.2) * 260
    end
    if dt > 0 then
        self.speed = math.sqrt((x - self.shipX) ^ 2 + (y - self.shipY) ^ 2) / dt
    end
    -- The ship turns toward where it flies once it moves fast enough, and keeps its heading when it stops.
    if self.speed > 60 then
        local target = math.atan(y - self.shipY, x - self.shipX)
        self.heading = self.heading + m.wrapAngle(target - self.heading) * m.dampFactor(8, dt)
    end
    self.shipX, self.shipY = x, y

    local upperX, upperY = self:place(MovingEmitters.nozzles[1])
    local lowerX, lowerY = self:place(MovingEmitters.nozzles[2])
    self.exhaust.x, self.exhaust.y = (upperX + lowerX) / 2, (upperY + lowerY) / 2
    self.exhaust.rotation = self.heading + self.rotation
    self.plain.x, self.plain.y = upperX, upperY
    self.spaced.x, self.spaced.y = lowerX, lowerY
    ParticleTest.updateAll(self.emitters, dt)
    self:report('The ship flies at %.0f units per second, and its exhaust inherits %.2f of that speed.', self.speed, self.inherit)
end

function MovingEmitters:draw(area)
    ParticleTest.backdrop({0.02, 0.03, 0.08}, {0.06, 0.05, 0.14})
    ParticleTest.drawAll(self.emitters)
    ParticleTest.prop('ship', self.shipX, self.shipY, MovingEmitters.scale, {rotation = self.heading, flipVertical = math.cos(self.heading) < 0, layer = 4})
    ParticleTest.legend(MovingEmitters.legend)
end

return MovingEmitters
