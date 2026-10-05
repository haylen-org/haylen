-- Something to pick up from the ground: a log from a fallen tree or meat from a sheep. It hops out of where it came from and waits to be collected.
local graphics2d = require('haylen.graphics2d')
local tween = require('haylen.tween')

local art = require('systems.art')
local config = require('config')

local pickup = {}
pickup.__index = pickup

local frames = {wood = 'log', meat = 'meat'}

function pickup.new(kind, x, y, targetX, targetY)
    local self = setmetatable({kind = kind, x = x, y = y, lift = 0, landed = false, time = math.random() * 3}, pickup)
    self.atlas = art.props()
    self.source = self.atlas:source(frames[kind])
    self.shadow = art.effects()
    self.shadowSource = self.shadow:source('shadow')
    -- The piece hops to where it lands, and only then can it be picked up.
    tween.to(self, 0.45, {x = targetX, y = targetY}, {ease = 'quadOut'})
    tween.timeline({onComplete = function()
        self.landed = true
    end}):append(tween.to(self, 0.2, {lift = 44}, {ease = 'quadOut'})):append(tween.to(self, 0.25, {lift = 0}, {ease = 'bounceOut'}))
    return self
end

function pickup:update(dt)
    self.time = self.time + dt
end

function pickup:draw()
    local bob = self.landed and math.sin(self.time * 3) * 4 or 0
    graphics2d.draw(self.shadow.texture, self.x, self.y + 4, {source = self.shadowSource, width = 56, height = 20, color = '#70FFFFFF', layer = config.layer.entities, depth = self.y - 2})
    graphics2d.draw(self.atlas.texture, self.x, self.y - self.lift - 6 + bob, {source = self.source, pivotX = 0.5, pivotY = 1, scaleX = 0.85, scaleY = 0.85, layer = config.layer.entities, depth = self.y})
end

return pickup
