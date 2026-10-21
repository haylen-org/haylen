-- Sparks: the disc of a grinder throws hot streaks from a cone, stretched along their motion, that damping slows and gravity pulls down until they bounce on the floor of a physics world.
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local particles2d = require('haylen.particles2d')
local physics2d = require('haylen.physics2d')
local ui = require('haylen.ui')

local ParticleTest = require('categories.particles.particle-test')

local Sparks = haylen.class('Sparks', ParticleTest)

Sparks.code = [[
local sparks = particles2d.newEmitter({texture = streak, rate = 300, lifetime = {0.5, 1.2}})
sparks:configure({speed = {500, 1000}, direction = -2.4, spread = 0.6, gravity = {0, 1400}})
sparks:configure({alignToVelocity = true, stretch = 0.035, blend = 'additive'})
sparks:configure({collision = {type = 'world', bounce = 0.4, sizeScale = 0}})
sparks:setCollisionWorld(world)  -- They bounce on the floor of the world.
sparks.emitting = input.down('fire')  -- Live sparks keep flying when it stops.]]

Sparks.floor = 400
-- The point of the disc of the grinder that touches the metal, as a share of its picture.
Sparks.contact = {85 / 429, 206 / 258}

function Sparks:init(entry)
    Sparks.super.init(self, entry)
    self.onlyHeld = false
    self.world = physics2d.newWorld()
    local floor = self.world:createBody({type = 'static', x = 0, y = Sparks.floor + 100})
    floor:addBox(8000, 200)
    self.sparks = particles2d.newEmitter(ParticleTest.look('spark-streak', {rate = 300, lifetime = {0.5, 1.2}, speed = {500, 1000}, direction = -2.4, spread = 0.6, gravity = {0, 1400}, damping = 1, startSize = {16, 24}, endSize = 6, alignToVelocity = true, stretch = 0.035, colors = {'#FFFFFFE0', '#FFFFD060', '#FFFF7010', '#00A02000'}, collision = {type = 'world', bounce = 0.4, friction = 0.2, sizeScale = 0}, shape = 'cone', shapeSize = {10, 0}, blend = 'additive', maxParticles = 2048, layer = 3, seed = 8}))
    self.sparks:setCollisionWorld(self.world)
    self.glow = particles2d.newEmitter(ParticleTest.look('soft-dot', {rate = 30, lifetime = 0.15, speed = 0, startSize = {110, 150}, endSize = 70, colors = {'#A0FFD080', '#00FF8020'}, blend = 'additive', layer = 4, seed = 9}))
    self.pointer.origin = {0.55, 0.55}
end

function Sparks:enter()
    self:frame{
        hint = 'The grinder follows the cursor. With the switch on, sparks fly only while you hold the mouse button, a finger, E, Enter or the south button.',
        code = Sparks.code,
        cursor = true,
        controls = {ui.toggle{id = 'held', text = 'Only while held', onChange = function(event)
            self.onlyHeld = event.checked
        end}},
    }
end

function Sparks:update(dt)
    Sparks.super.update(self, dt)
    local on = not self.onlyHeld or self:held()
    for _, emitter in ipairs({self.sparks, self.glow}) do
        emitter.position = {self.cursorX, self.cursorY}
        emitter.emitting = on
        emitter:update(dt)
    end
    self:report(on and '%d sparks fly from the disc.' or '%d sparks still fly, and the grinder is off.', self.sparks.count)
end

function Sparks:draw(area)
    ParticleTest.backdrop({0.07, 0.07, 0.1}, {0.16, 0.15, 0.18})
    ParticleTest.paveFloor('stone', Sparks.floor, 150, '#FF8A8C98')
    ParticleTest.prop('grinder', self.cursorX, self.cursorY, 0.6, {pivotX = Sparks.contact[1], pivotY = Sparks.contact[2], layer = 2})
    self.sparks:draw()
    self.glow:draw()
end

return Sparks
