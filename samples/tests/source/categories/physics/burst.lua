-- An effect of the particle library of the project fired where something happens, such as the sparks of a hit, the dust of a landing or a blast: a few systems of the effect taken in turn, so a burst never makes a new one and the oldest gives way when they are all busy.
local assets = require('haylen.assets')
local haylen = require('haylen')
local particles2d = require('haylen.particles2d')

local Burst = haylen.class('Burst')

Burst.folder = 'particles/library/'

-- The argument `effect` is the path of an effect in the library, such as `'hits/hit-light'`, and `options` takes how many fire at once, its `count`, 4 by default, the `scale` of the effect, the `layer` it draws at and a `seed`.
function Burst:init(effect, options)
    options = options or {}
    local loaded = assets.load(Burst.folder .. effect .. '.particles')
    self.scale = options.scale or 1
    self.systems = {}
    for index = 1, options.count or 4 do
        local system = particles2d.newSystem(loaded, {seed = (options.seed or 1) + index * 7})
        for _, emitter in ipairs(system:emitters()) do
            emitter:configure({layer = options.layer or 30})
        end
        system.emitting = false
        system:clear()
        self.systems[index] = system
    end
    self.turn = 1
end

-- Fires the effect at `x`, `y`, at `scale` times the scale of the burst and turned by `rotation`.
function Burst:fire(x, y, scale, rotation)
    local system = self.systems[self.turn]
    self.turn = self.turn % #self.systems + 1
    system.x, system.y = x, y
    system.scale = self.scale * (scale or 1)
    system.rotation = rotation or 0
    system:restart()
end

function Burst:update(dt)
    for _, system in ipairs(self.systems) do
        if system.alive then
            system:update(dt)
        end
    end
end

function Burst:draw()
    for _, system in ipairs(self.systems) do
        if system.alive then
            system:draw()
        end
    end
end

return Burst
