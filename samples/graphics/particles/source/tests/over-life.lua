-- Color, size and frames over the lifetime: colors spread evenly over the life and blend between, the size moves from `startSize` to `endSize`, and frames play from birth to death.
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local particles2d = require('haylen.particles2d')

local art = require('art')
local sample = require('sample')

local OverLife = haylen.class('OverLife', sample.Test)

OverLife.hints = 'Each column changes one thing over the lifetime of its particles.'

function OverLife:init(entry)
    OverLife.super.init(self, entry)
    self.camera = graphics2d.newCamera()
    local soft, sparkle, smoke = art.texture('soft'), art.texture('sparkle'), art.texture('smoke')
    local rise = {rate = 30, lifetime = 2.4, speed = {110, 130}, spread = 0.15, layer = 2}
    local columns = {
        {label = 'Five colors', options = {texture = soft, startSize = 40, endSize = 40, colors = {'#FFFF4040', '#FFFFC040', '#FF40FF80', '#FF40A0FF', '#00C060FF'}, blend = 'additive'}},
        {label = 'Size 8 to 90', options = {texture = soft, startSize = 8, endSize = 90, colors = {'#FFFFFFFF', '#40FFFFFF'}}},
        {label = 'Size 90 to 0', options = {texture = soft, startSize = 90, endSize = 0, colors = {'#FFFFB060'}}},
        {label = 'Six frames', options = {texture = sparkle, frames = art.frames(sparkle), startSize = 60, endSize = 60, colors = {'#FFFFF0A0'}, blend = 'additive'}},
        {label = 'Smoke frames', options = {texture = smoke, frames = art.frames(smoke), startSize = 50, endSize = 120, spin = {-1, 1}, colors = {'#00B0B0C0', '#C0B0B0C0', '#00B0B0C0'}}},
    }
    self.columns = {}
    for index, column in ipairs(columns) do
        for key, value in pairs(rise) do
            column.options[key] = value
        end
        column.options.seed = 70 + index
        column.emitter = particles2d.newEmitter(column.options)
        column.emitter.position = {(index - 3) * 330, 330}
        self.columns[index] = column
    end
end

function OverLife:update(dt)
    local count = 0
    for _, column in ipairs(self.columns) do
        column.emitter:update(dt)
        count = count + column.emitter.count
    end
    self:setStatus(string.format('%d particles over %d emitters', count, #self.columns))
end

function OverLife:render()
    graphics2d.beginWorld(self.camera)
    art.backdrop({0.05, 0.05, 0.1}, {0.12, 0.1, 0.18})
    for _, column in ipairs(self.columns) do
        column.emitter:draw()
    end
end

function OverLife:renderUi()
    graphics2d.beginScreen()
    for _, column in ipairs(self.columns) do
        art.label(self.camera, column.label, column.emitter.x, 360)
    end
end

return OverLife
