-- Composite effects: a `.particles` file whose only key is `emitters` lists the parts of one effect, each another effect file or its own options, with an offset, a scale, a delay and a layer offset. The function `particles2d.newSystem` creates them together, the system moves, scales and turns them as one, and `system:emitter(name)` returns a part by its name, such as the smoke of the campfire to switch off.
local assets = require('haylen.assets')
local haylen = require('haylen')
local particles2d = require('haylen.particles2d')
local ui = require('haylen.ui')

local ParticleTest = require('categories.particles.particle-test')

local CompositeEffects = haylen.class('CompositeEffects', ParticleTest)

CompositeEffects.code = [[
local effect = assets.load('particles/composites/campfire.particles')
local campfire = particles2d.newSystem(effect, {seed = 311})
campfire.x, campfire.y = x, y
campfire.scale, campfire.rotation = 1.5, 0.3  -- Every part scales and turns with it.
campfire:emitter('smoke').emitting = false  -- A part by its name.
explosion:restart()  -- Plays every part again.]]

CompositeEffects.spots = {{-560, -60}, {520, 120}, {-200, 220}, {300, -220}}
CompositeEffects.interval = 2.2
CompositeEffects.floor = 360

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
    self.legends = {CompositeEffects.legend('Explosion', explosion, '#FFFF9040'), CompositeEffects.legend('Campfire', campfire, '#FFFFD060')}
    self.pointer.origin = {0.5, 0.78}
end

-- Names the parts of a composite effect in the order they draw, as a line of the legend.
function CompositeEffects.legend(title, effect, color)
    local names = {}
    for index, part in ipairs(effect.parts) do
        names[index] = part.name
    end
    return {text = string.format('%s, %d parts: %s', title, #names, table.concat(names, ', ')), color = color}
end

function CompositeEffects:explode(x, y)
    self.clock = 0
    self.explosion.x, self.explosion.y = x, y
    self.explosion:restart()
end

function CompositeEffects:enter()
    self:frame{
        hint = 'The campfire follows the cursor: move it with the mouse, a finger, the arrows, WASD or a stick. A click, a tap, E, Enter, the south button or the button of the panel sets off the explosion at the cursor, and it also goes off by itself.',
        code = CompositeEffects.code,
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
    self:report('The campfire burns with %d particles and the explosion with %d, scaled by %.2f and turned by %.2f.', self.campfire.count, self.explosion.count, self.scale, self.rotation)
end

-- Draws the logs and stones of the campfire under its flames, turned and scaled with it.
function CompositeEffects:drawLogs()
    local scale = self.scale
    local x, y = self.campfire.x - math.sin(self.rotation) * 30 * scale, self.campfire.y + math.cos(self.rotation) * 30 * scale
    ParticleTest.prop('campfire', x, y, 0.42 * scale, {rotation = self.rotation, layer = 1})
end

function CompositeEffects:draw(area)
    ParticleTest.backdrop({0.04, 0.05, 0.11}, {0.14, 0.11, 0.16})
    ParticleTest.paveFloor('dirt', CompositeEffects.floor, 150, '#FF6A5A58')
    self:drawLogs()
    ParticleTest.drawAll(self.systems)
    ParticleTest.legend(self.legends)
end

return CompositeEffects
