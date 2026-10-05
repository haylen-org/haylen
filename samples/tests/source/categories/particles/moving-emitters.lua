-- Moving emitters: an emitter measures how fast it moves between updates, so its exhaust takes the share `inheritVelocity` of that velocity, a trail with `rateOverDistance` spawns along the way it moved and leaves no gaps where a trail with only a `rate` does, and its `scale` and `rotation` resize and turn what it spawns from then on.
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local particles2d = require('haylen.particles2d')
local ui = require('haylen.ui')

local ParticleTest = require('categories.particles.particle-test')

local MovingEmitters = haylen.class('MovingEmitters', ParticleTest)

MovingEmitters.wing = 130
MovingEmitters.hull = {layer = 4}
MovingEmitters.legendStyle = {size = 26, anchor = {0, 0.5}, outlineWidth = 3, layer = 20}

function MovingEmitters:init(entry)
    MovingEmitters.super.init(self, entry)
    self.inherit = 0.8
    self.scale = 1
    self.rotation = 0
    self.autopilot = false
    self.time = 0
    self.shipX, self.shipY = 0, 0
    self.speed = 0
    local dot = ParticleTest.library('soft_dot')

    self.exhaust = particles2d.newEmitter({texture = ParticleTest.library('flame_soft'), rate = 140, lifetime = {0.5, 0.8}, speed = {260, 340}, direction = 1.5708, spread = 0.25, inheritVelocity = self.inherit, startSize = {44, 56}, endSize = 6, colors = {'#FFFFFFE0', '#FFFFB040', '#C0FF5020', '#00602010'}, blend = 'additive', maxParticles = 256, layer = 3, seed = 271})
    self.plain = particles2d.newEmitter({texture = dot, rate = 60, lifetime = 1.2, speed = 0, startSize = 26, endSize = 4, colors = {'#FFFFB040', '#00FF6020'}, blend = 'additive', maxParticles = 256, layer = 2, seed = 272})
    self.spaced = particles2d.newEmitter({texture = dot, rate = 0, rateOverDistance = 0.25, lifetime = 1.2, speed = 0, startSize = 26, endSize = 4, colors = {'#FF60E0FF', '#002080FF'}, blend = 'additive', maxParticles = 2048, layer = 2, seed = 273})
    self.emitters = {self.exhaust, self.plain, self.spaced}
end

function MovingEmitters:enter()
    self:frame{
        hint = 'The ship follows the cursor: move it fast with the mouse, a finger, the arrows, WASD or a stick, or turn on the autopilot. The orange trail spawns by time and breaks up at speed, and the blue one spawns by distance.',
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
            ui.formField{label = 'Rotation of the exhaust', ui.slider{id = 'rotation', min = -3.1416, max = 3.1416, value = self.rotation, showValue = true, onChange = function(event)
                self.rotation = event.value
                self.exhaust.rotation = event.value
            end}},
            ui.toggle{id = 'autopilot', text = 'Autopilot on a figure eight', checked = self.autopilot, onChange = function(event)
                self.autopilot = event.checked
            end},
        },
    }
end

function MovingEmitters:update(dt)
    MovingEmitters.super.update(self, dt)
    local x, y = self.cursorX, self.cursorY
    if self.autopilot then
        self.time = self.time + dt
        x, y = math.sin(self.time * 1.6) * 640, math.sin(self.time * 3.2) * 260
    end
    if dt > 0 then
        self.speed = math.sqrt((x - self.shipX) ^ 2 + (y - self.shipY) ^ 2) / dt
    end
    self.shipX, self.shipY = x, y

    self.exhaust.x, self.exhaust.y = x, y + 30
    self.plain.x, self.plain.y = x - MovingEmitters.wing, y
    self.spaced.x, self.spaced.y = x + MovingEmitters.wing, y
    ParticleTest.updateAll(self.emitters, dt)
    self:report('Ship speed %.0f   Exhaust %d with "inheritVelocity" %.2f, scale %.2f and rotation %.2f   Trail by "rate" %d   Trail by "rateOverDistance" %d', self.speed, self.exhaust.count, self.inherit, self.scale, self.rotation, self.plain.count, self.spaced.count)
end

function MovingEmitters:draw(area)
    ParticleTest.backdrop({0.02, 0.03, 0.08}, {0.06, 0.05, 0.14})
    ParticleTest.drawAll(self.emitters)
    local x, y = self.shipX, self.shipY
    graphics2d.drawLine(x - MovingEmitters.wing, y, x + MovingEmitters.wing, y, 8, '#FF8090B0', MovingEmitters.hull)
    graphics2d.drawCircle(x, y, 34, '#FFD0D8F0', MovingEmitters.hull)
    graphics2d.drawCircle(x - MovingEmitters.wing, y, 10, '#FFFFB040', MovingEmitters.hull)
    graphics2d.drawCircle(x + MovingEmitters.wing, y, 10, '#FF60E0FF', MovingEmitters.hull)
    graphics2d.drawText(nil, 'Orange trail: 60 particles per second with "rate"', -900, -480, MovingEmitters.legendStyle)
    graphics2d.drawText(nil, 'Blue trail: one particle every 4 units with "rateOverDistance"', -900, -430, MovingEmitters.legendStyle)
end

return MovingEmitters
