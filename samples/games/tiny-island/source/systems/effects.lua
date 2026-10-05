-- Short visual feedback in the world: particle bursts, one-shot animations such as slashes, thrusts and explosions, and floating numbers.
local assets = require('haylen.assets')
local graphics2d = require('haylen.graphics2d')
local particles2d = require('haylen.particles2d')

local art = require('systems.art')
local config = require('config')

local effects = {}
effects.__index = effects

local bursts = {'sparks', 'chips', 'dust', 'smoke', 'embers', 'wool', 'flames'}

-- Frames per second of every one-shot animation of the effects atlas.
local animations = {slash = 16, thrust = 16, explosion = 16, hit = 22, splash = 14}

function effects.new()
    local self = setmetatable({emitters = {}, clips = {}, playing = {}, numbers = {}, font = assets.font('fonts/lilita_one_regular.ttf')}, effects)
    for _, name in ipairs(bursts) do
        self.emitters[name] = particles2d.newEmitter(assets.load('effects/' .. name .. '.particles', nil, art.options))
    end
    for name, framesPerSecond in pairs(animations) do
        self.clips[name] = art.effect(name, framesPerSecond)
    end
    return self
end

function effects:burst(name, x, y, count)
    local emitter = self.emitters[name]
    emitter.x = x
    emitter.y = y
    emitter:burst(count)
end

-- Plays a one-shot animation at a point. The table `options` may set `rotation`, `scale`, `color` and `glow`, which keeps the colors bright at night.
function effects:animate(name, x, y, options)
    options = options or {}
    local clip = self.clips[name]
    local scale = options.scale or 1
    local sprite = graphics2d.newSprite(clip.texture, {x = x, y = y, rotation = options.rotation or 0, scaleX = scale, scaleY = scale, color = options.color or '#FFFFFFFF', layer = config.layer.effects, depth = y, unshaded = options.glow or false})
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
        local rise = number.time * 70
        graphics2d.drawText(self.font, number.text, number.x, number.y - 40 - rise, {size = 34, color = alpha, outlineWidth = 4, outlineColor = string.format('#%02X1E1B33', math.floor(fade * 255)), anchor = {0.5, 1}, layer = config.layer.overlay})
    end
end

return effects
