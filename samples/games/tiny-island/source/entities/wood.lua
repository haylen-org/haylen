-- A piece of wood on the ground. It pops out of a fallen tree and waits to be picked up.
local graphics2d = require('haylen.graphics2d')
local tween = require('haylen.tween')

local art = require('systems.art')
local config = require('config')

local wood = {}
wood.__index = wood

function wood.new(x, y, targetX, targetY)
    local self = setmetatable({x = x, y = y, lift = 0, landed = false, time = math.random() * 3}, wood)
    self.texture = art.texture('terrain/resources/wood/wood_resource/wood_resource.png')
    -- The piece hops from the trunk to where it lands, and only then can it be picked up.
    tween.to(self, 0.45, {x = targetX, y = targetY}, {ease = 'quad_out'})
    tween.timeline({onComplete = function()
        self.landed = true
    end}):append(tween.to(self, 0.2, {lift = 40}, {ease = 'quad_out'})):append(tween.to(self, 0.25, {lift = 0}, {ease = 'bounce_out'}))
    return self
end

function wood:update(dt)
    self.time = self.time + dt
end

function wood:draw()
    local bob = math.sin(self.time * 3) * 3
    graphics2d.drawCircle(self.x, self.y + 6, 18, '#40000000', {layer = config.layer.entities, depth = self.y - 2})
    graphics2d.draw(self.texture, self.x, self.y - self.lift + bob, {pivotX = 0.5, pivotY = 0.8, scaleX = 0.8, scaleY = 0.8, layer = config.layer.entities, depth = self.y})
end

return wood
