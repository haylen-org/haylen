-- Composite effects: a `.particles` file whose only key is `emitters` lists the parts of one effect, each another effect file or its own options, with an offset, a scale, a delay and a layer offset. The function `particles2d.newSystem` creates them together, the system moves, scales and turns them as one, and `system:emitter(name)` returns a part by its name, such as the smoke of the campfire to switch off.
local assets = require('haylen.assets')
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local particles2d = require('haylen.particles2d')
local ui = require('haylen.ui')

local ParticleTest = require('categories.particles.particle-test')

local CompositeEffects = haylen.class('CompositeEffects', ParticleTest)

CompositeEffects.spots = {{-560, -60}, {520, 120}, {-200, 220}, {300, -220}}
CompositeEffects.interval = 2.2
CompositeEffects.logs = {layer = 1}
CompositeEffects.logAngles = {0.35, -0.35}

function CompositeEffects:init(entry)
    CompositeEffects.super.init(self, entry)
    self.scale = 1
    self.rotation = 0
    self.smoking = true
    self.clock = 0
    self.spot = 1

    local explosion = assets.load('particles/composites/explosion.particles')
    local campfire = assets.load('particles/composites/campfire.particles')
    self.explosion = particles2d.newSystem(explosion, {seed = 301})
    self.campfire = particles2d.newSystem(campfire, {seed = 311})
    self.campfireSmoke = self.campfire:emitter('smoke')
    self.explosion.x, self.explosion.y = CompositeEffects.spots[1][1], CompositeEffects.spots[1][2]
    self.systems = {self.explosion, self.campfire}
    self.legends = {CompositeEffects.legend('Explosion', explosion), CompositeEffects.legend('Campfire', campfire)}
end

-- Names the parts of a composite effect in the order they draw.
function CompositeEffects.legend(title, effect)
    local names = {}
    for index, part in ipairs(effect.parts) do
        names[index] = '"' .. part.name .. '"'
    end
    return string.format('%s with %d parts: %s', title, #names, table.concat(names, ', '))
end

function CompositeEffects:explode(x, y)
    self.clock = 0
    self.explosion.x, self.explosion.y = x, y
    self.explosion:restart()
end

function CompositeEffects:enter()
    self:frame{
        hint = 'The campfire follows the cursor: move it with the mouse, a finger, the arrows, WASD or a stick. A click, a tap, E, Enter, the south button or the button of the panel sets off the explosion at the cursor, and it also goes off by itself.',
        cursor = true,
        controls = {
            ui.button{id = 'explode', text = 'Set off the explosion', onClick = function()
                self:explode(self.cursorX, self.cursorY)
            end},
            ui.formField{label = 'Scale of both effects', ui.slider{id = 'scale', min = 0.3, max = 2, value = self.scale, showValue = true, onChange = function(event)
                self.scale = event.value
                for _, system in ipairs(self.systems) do
                    system.scale = event.value
                end
            end}},
            ui.formField{label = 'Rotation of both effects', ui.slider{id = 'rotation', min = -3.1416, max = 3.1416, value = self.rotation, showValue = true, onChange = function(event)
                self.rotation = event.value
                for _, system in ipairs(self.systems) do
                    system.rotation = event.value
                end
            end}},
            ui.toggle{id = 'smoke', text = 'Smoke of the campfire', checked = self.smoking, onChange = function(event)
                self.smoking = event.checked
                self.campfireSmoke.emitting = event.checked
            end},
        },
    }
end

function CompositeEffects:update(dt)
    CompositeEffects.super.update(self, dt)
    self.clock = self.clock + dt
    if self:pressed() then
        self:explode(self.cursorX, self.cursorY)
    elseif self.clock > CompositeEffects.interval then
        self.spot = self.spot % #CompositeEffects.spots + 1
        local spot = CompositeEffects.spots[self.spot]
        self:explode(spot[1], spot[2])
    end
    self.campfire.x, self.campfire.y = self.cursorX, self.cursorY
    ParticleTest.updateAll(self.systems, dt)
    self:report('Campfire %d particles with smoke "%s"   Explosion %d particles, alive "%s"   Scale %.2f   Rotation %.2f', self.campfire.count, self.smoking, self.explosion.count, self.explosion.alive, self.scale, self.rotation)
end

-- Draws two crossed logs under the campfire, turned and scaled with it.
function CompositeEffects:drawLogs()
    local scale = self.scale
    local x, y = self.campfire.x - math.sin(self.rotation) * 10 * scale, self.campfire.y + math.cos(self.rotation) * 10 * scale
    for _, angle in ipairs(CompositeEffects.logAngles) do
        local turn = angle + self.rotation
        local dx, dy = math.cos(turn) * 70 * scale, math.sin(turn) * 70 * scale
        graphics2d.drawLine(x - dx, y - dy, x + dx, y + dy, 18 * scale, '#FF6A4228', CompositeEffects.logs)
    end
end

function CompositeEffects:draw(area)
    ParticleTest.backdrop({0.03, 0.04, 0.09}, {0.12, 0.1, 0.14})
    self:drawLogs()
    ParticleTest.drawAll(self.systems)
    ParticleTest.label(self.legends[1], 0, -500)
    ParticleTest.label(self.legends[2], 0, -455)
end

return CompositeEffects
