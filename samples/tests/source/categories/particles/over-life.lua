-- Color, size and frames over the lifetime: colors spread evenly over the life and blend between, the size moves from `startSize` to `endSize`, and frames play from birth to death.
local haylen = require('haylen')
local particles2d = require('haylen.particles2d')

local ParticleTest = require('categories.particles.particle-test')

local OverLife = haylen.class('OverLife', ParticleTest)

OverLife.code = [[
local colors = {'#FFFF4040', '#FFFFC040', '#FF40FF80', '#FF40A0FF', '#00C060FF'}
local fade = particles2d.newEmitter({texture = dot, lifetime = 3.2, colors = colors})
local grow = particles2d.newEmitter({texture = dot, lifetime = 3.2, startSize = 8, endSize = 90})
local frames = {atlas:source('sparkle-1'), atlas:source('sparkle-2'), atlas:source('sparkle-3')}
-- The frames play once from birth to death.
local twinkle = particles2d.newEmitter({texture = atlas.texture, frames = frames})]]

OverLife.rise = {rate = 26, lifetime = 3.2, speed = {120, 140}, spread = 0.15, layer = 2}

function OverLife:init(entry)
    OverLife.super.init(self, entry)
    local soft, sparkle, smoke = 'soft-dot', 'sparkle', 'smoke'
    self.columns = {
        {label = 'Five colors', options = ParticleTest.look(soft, {startSize = 40, endSize = 40, colors = {'#FFFF4040', '#FFFFC040', '#FF40FF80', '#FF40A0FF', '#00C060FF'}, blend = 'additive'})},
        {label = 'Size 8 to 90', options = ParticleTest.look(soft, {startSize = 8, endSize = 90, colors = {'#FFFFFFFF', '#40FFFFFF'}})},
        {label = 'Size 90 to 0', options = ParticleTest.look(soft, {startSize = 90, endSize = 0, colors = {'#FFFFB060'}})},
        {label = 'Four frames', options = ParticleTest.look(sparkle, {startSize = 60, endSize = 60, colors = {'#FFFFF0A0'}, blend = 'additive'})},
        {label = 'Smoke frames', options = ParticleTest.look(smoke, {startSize = 50, endSize = 120, spin = {-1, 1}, colors = {'#00B0B0C0', '#C0B0B0C0', '#00B0B0C0'}})},
    }
    for index, column in ipairs(self.columns) do
        for key, value in pairs(OverLife.rise) do
            column.options[key] = value
        end
        column.options.seed = 70 + index
        column.emitter = particles2d.newEmitter(column.options)
        column.emitter.position = {(index - 3) * 340, 250}
    end
end

function OverLife:enter()
    self:frame{hint = 'Each column changes one thing over the lifetime of its particles.', code = OverLife.code}
end

function OverLife:update(dt)
    OverLife.super.update(self, dt)
    local count = 0
    for _, column in ipairs(self.columns) do
        column.emitter:update(dt)
        count = count + column.emitter.count
    end
    self:report('%d particles in %d columns, each changing one thing over its life.', count, #self.columns)
end

function OverLife:draw(area)
    ParticleTest.backdrop({0.05, 0.05, 0.1}, {0.12, 0.1, 0.18})
    for _, column in ipairs(self.columns) do
        column.emitter:draw()
        ParticleTest.label(column.label, column.emitter.x, 290)
    end
end

return OverLife
