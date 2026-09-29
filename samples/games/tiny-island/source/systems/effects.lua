-- Short visual feedback in the world: particle bursts, one-shot animations and floating numbers.
local assets = require('haylen.assets')
local graphics2d = require('haylen.graphics2d')
local particles2d = require('haylen.particles2d')

local art = require('systems.art')
local config = require('config')

local effects = {}
effects.__index = effects

local bursts = {chips = 'effects/wood_chips.particles', heal = 'effects/heal.particles', sparks = 'effects/sparks.particles', dust = 'effects/dust.particles'}

local animations = {
    explosion = {path = 'effects/explosion_01.png', frame = 192, fps = 16},
    bigExplosion = {path = 'effects/explosion_02.png', frame = 192, fps = 16},
    splash = {path = 'effects/water_splash.png', frame = 192, fps = 16},
}

function effects.new()
    local self = setmetatable({emitters = {}, playing = {}, numbers = {}}, effects)
    for name, path in pairs(bursts) do
        self.emitters[name] = particles2d.newEmitter(assets.load(path))
    end
    self.clips = {}
    for name, spec in pairs(animations) do
        self.clips[name] = art.strip(spec.path, spec.frame, {fps = spec.fps, loop = false})
    end
    return self
end

function effects:burst(name, x, y, count)
    local emitter = self.emitters[name]
    emitter.x = x
    emitter.y = y
    emitter:burst(count)
end

function effects:animate(name, x, y, scale)
    local clip = self.clips[name]
    local sprite = graphics2d.newSprite(clip.texture, {x = x, y = y, pivotX = 0.5, pivotY = 0.5, scaleX = scale or 1, scaleY = scale or 1, layer = config.layer.effects, depth = y})
    self.playing[#self.playing + 1] = {clip = clip, sprite = sprite, time = 0}
end

function effects:number(x, y, text, color)
    self.numbers[#self.numbers + 1] = {x = x, y = y, text = text, color = color, time = 0}
end

function effects:update(dt)
    for _, emitter in pairs(self.emitters) do
        emitter:update(dt)
    end
    for index = #self.playing, 1, -1 do
        local item = self.playing[index]
        item.time = item.time + dt
        if item.time >= item.clip.duration then
            table.remove(self.playing, index)
        end
    end
    for index = #self.numbers, 1, -1 do
        local number = self.numbers[index]
        number.time = number.time + dt
        if number.time >= 0.9 then
            table.remove(self.numbers, index)
        end
    end
end

function effects:draw()
    for _, emitter in pairs(self.emitters) do
        emitter:draw()
    end
    for _, item in ipairs(self.playing) do
        item.sprite.source = item.clip:frame(item.clip:frameAt(item.time))
        item.sprite:draw()
    end
    for _, number in ipairs(self.numbers) do
        local fade = 1 - number.time / 0.9
        local color = number.color or '#FFFFFFFF'
        local alpha = string.format('#%02X%s', math.floor(fade * 255), color:sub(4))
        graphics2d.drawText(nil, number.text, number.x, number.y - 50 - number.time * 60, {size = 30, color = alpha, outlineWidth = 3, outlineColor = string.format('#%02X1B1E2B', math.floor(fade * 255)), anchor = {0.5, 1}, layer = config.layer.overlay})
    end
end

return effects
