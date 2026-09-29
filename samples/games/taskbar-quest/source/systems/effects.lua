-- Short feedback in the strip: bursts of square pixel particles and words that float up and fade.
local graphics = require('haylen.graphics')
local graphics2d = require('haylen.graphics2d')
local m = require('haylen.math')
local particles2d = require('haylen.particles2d')

local config = require('config')

local effects = {}
effects.__index = effects

local textLife = 0.9
local ink = m.color('#FF1A1C2C')

local bursts = {
    goo = {colors = {'#FF38B764', '#FFA7F070', '#C0257179'}, speed = {80, 200}, gravity = {0, 560}},
    spores = {colors = {'#FFD33A4A', '#FFF4F4F4', '#C0EFD8A1'}, speed = {70, 180}, gravity = {0, 480}},
    sparks = {colors = {'#FFFFFFFF', '#FFFFCD75', '#00F2A93B'}, speed = {90, 220}, gravity = {0, 300}},
    dust = {colors = {'#E0C69C6D', '#808F563B', '#008F563B'}, speed = {30, 90}, gravity = {0, -40}},
    heal = {colors = {'#FFA7F070', '#FFFFFFFF', '#0038B764'}, speed = {30, 80}, gravity = {0, -160}},
}

function effects.new()
    local self = setmetatable({emitters = {}, texts = {}}, effects)
    for name, burst in pairs(bursts) do
        self.emitters[name] = particles2d.newEmitter({
            texture = graphics.whiteTexture(),
            rate = 0,
            lifetime = {0.35, 0.75},
            speed = burst.speed,
            direction = -math.pi / 2,
            spread = 2.6,
            gravity = burst.gravity,
            startSize = {2 * config.pixel, 3 * config.pixel},
            endSize = config.pixel,
            colors = burst.colors,
            maxParticles = 200,
            layer = config.layer.effects,
        })
    end
    return self
end

function effects:burst(name, x, y, count)
    local emitter = self.emitters[name]
    emitter.x = x
    emitter.y = y
    emitter:burst(count)
end

function effects:text(x, y, text, color)
    self.texts[#self.texts + 1] = {x = x, y = y, text = text, color = m.color(color), time = 0}
end

function effects:update(dt)
    for _, emitter in pairs(self.emitters) do
        emitter:update(dt)
    end
    for index = #self.texts, 1, -1 do
        local item = self.texts[index]
        item.time = item.time + dt
        if item.time >= textLife then
            table.remove(self.texts, index)
        end
    end
end

function effects:draw()
    for _, emitter in pairs(self.emitters) do
        emitter:draw()
    end
    for _, item in ipairs(self.texts) do
        local progress = item.time / textLife
        local alpha = 1 - m.smoothstep(0.6, 1, progress)
        graphics2d.drawText(nil, item.text, item.x, item.y - m.ease('quad_out', progress) * 36, {
            size = 26,
            color = item.color:withAlpha(alpha),
            outlineWidth = 3,
            outlineColor = ink:withAlpha(alpha),
            anchor = {0.5, 1},
            layer = config.layer.text,
        })
    end
end

return effects
