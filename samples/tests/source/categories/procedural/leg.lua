-- A leg of a procedural creature: a chain of two segments from the hip that reaches its foot, and a foot that stays where it stands until the gait moves it to a new foothold in an arc. A creature places the hip, the rest point of the foot and the pole of the knee every frame, and `Leg.gait` decides which feet step.
local animation2d = require('haylen.animation2d')
local collections = require('haylen.collections')
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')

local Leg = haylen.class('Leg')

-- Takes the hip `x`, `y`, the `lengths` of the thigh and the shin, the `group` of the gait it steps with, the `stepTime` and `lift` of a step and the `widths` and `color` of its stroke.
function Leg:init(options)
    self.chain = animation2d.newChain({x = options.x, y = options.y, lengths = options.lengths, angle = options.angle or math.pi / 2})
    self.footX, self.footY = self.chain:point(3)
    self.restX, self.restY = self.footX, self.footY
    self.group = options.group or 1
    self.stepTime = options.stepTime or 0.12
    self.lift = options.lift or 12
    self.reachOptions = {pole = {0, 0}}
    self.points = collections.newFloatBuffer(6)
    self.stroke = {widths = options.widths or {6, 5, 3}, color = options.color or '#FF20242E', join = 'round', cap = 'round', layer = options.layer or 5}
end

function Leg:stepping()
    return self.progress ~= nil
end

function Leg:stepTo(x, y)
    self.fromX, self.fromY = self.footX, self.footY
    self.toX, self.toY = x, y
    self.progress = 0
end

-- Puts the foot down at once, such as when a creature lands or tucks its legs in a leap.
function Leg:place(x, y)
    self.progress = nil
    self.footX, self.footY = x, y
end

-- Moves a step on, lifting the foot along `liftX`, `liftY`, the direction away from the ground, and bends the leg from the hip to the foot with its knee toward the pole.
function Leg:update(dt, hipX, hipY, liftX, liftY, poleX, poleY)
    if self.progress then
        self.progress = math.min(1, self.progress + dt / self.stepTime)
        local t = self.progress
        local eased = t * t * (3 - 2 * t)
        local arc = math.sin(t * math.pi) * self.lift
        self.footX = self.fromX + (self.toX - self.fromX) * eased + liftX * arc
        self.footY = self.fromY + (self.toY - self.fromY) * eased + liftY * arc
        if t >= 1 then
            self.progress = nil
        end
    end
    self.chain.x, self.chain.y = hipX, hipY
    local pole = self.reachOptions.pole
    pole[1], pole[2] = poleX, poleY
    self.chain:reach(self.footX, self.footY, self.reachOptions)
end

function Leg:draw(color, layer)
    self.chain:writePoints(self.points)
    self.stroke.color = color or self.stroke.color
    self.stroke.layer = layer or self.stroke.layer
    graphics2d.drawStroke(self.points, self.stroke)
end

-- Moves the feet that fell behind: a planted leg whose foot is farther than `stride` from its rest point steps to its rest point plus the lead `leadX`, `leadY`, or the `leadX` and `leadY` of the leg when it has its own, placed on the ground by `ground(x, y)` when given, while no leg of another group steps, so the groups take turns and the creature always stands on the others. A foot more than three strides away steps at once whatever the others do.
function Leg.gait(legs, stride, leadX, leadY, ground)
    local busy
    for _, leg in ipairs(legs) do
        if leg:stepping() then
            busy = leg.group
        end
    end
    for _, leg in ipairs(legs) do
        if not leg:stepping() then
            local dx, dy = leg.footX - leg.restX, leg.footY - leg.restY
            local distance = math.sqrt(dx * dx + dy * dy)
            if distance > stride * 3 or (distance > stride and (busy == nil or busy == leg.group)) then
                local x, y = leg.restX + (leg.leadX or leadX), leg.restY + (leg.leadY or leadY)
                if ground then
                    x, y = ground(x, y)
                end
                leg:stepTo(x, y)
                busy = busy or leg.group
            end
        end
    end
end

return Leg
