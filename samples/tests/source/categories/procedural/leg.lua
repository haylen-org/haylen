-- A leg of a procedural creature: a chain of segments from the hip that reaches its foot, and a foot that stays where it stands until the gait moves it to a new foothold in an arc. A creature places the hip, the rest point of the foot and the pole of the knee every frame, and `Leg.gait` or `Leg.wave` decides which feet step.
local animation2d = require('haylen.animation2d')
local collections = require('haylen.collections')
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')

local Leg = haylen.class('Leg')

-- Takes the hip `x`, `y`, the `lengths` of its segments from the hip on, the length of a `tarsus` that angles from the foot up and out to the ankle the segments reach, when it has one, the `group` of `Leg.gait` or the `phase` of `Leg.wave` it steps with, the `stepTime` and `lift` of a step and the `widths` of each point and `color` of its stroke.
function Leg:init(options)
    self.chain = animation2d.newChain({x = options.x, y = options.y, lengths = options.lengths, angle = options.angle or math.pi / 2})
    self.footX, self.footY = self.chain:point(self.chain.count)
    self.restX, self.restY = self.footX, self.footY
    self.group = options.group or 1
    self.stepTime = options.stepTime or 0.12
    self.lift = options.lift or 12
    self.reachOptions = {pole = {0, 0}}
    self.phase = options.phase or 0
    self.tarsus = options.tarsus
    self.ankleX, self.ankleY = self.footX, self.footY
    self.points = collections.newFloatBuffer((#options.lengths + (self.tarsus and 2 or 1)) * 2)
    self.stroke = {widths = options.widths or {6, 5, 3}, color = options.color or '#FF20242E', join = 'round', cap = 'round', layer = options.layer or 5}
end

function Leg:stepping()
    return self.progress ~= nil
end

function Leg:stepTo(x, y, time)
    self.duration = time or self.stepTime
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
        self.progress = math.min(1, self.progress + dt / self.duration)
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
    self.ankleX, self.ankleY = self.footX, self.footY
    -- The tarsus rises from the foot mostly away from the ground and a little away from the hip, the way a spider stands on the tips of its legs.
    if self.tarsus then
        local outX, outY = self.footX - hipX, self.footY - hipY
        local along = outX * liftX + outY * liftY
        outX, outY = outX - liftX * along, outY - liftY * along
        local length = math.max(1e-4, math.sqrt(outX * outX + outY * outY))
        self.ankleX = self.footX + (liftX * 0.85 + outX / length * 0.5) * self.tarsus
        self.ankleY = self.footY + (liftY * 0.85 + outY / length * 0.5) * self.tarsus
    end
    self.chain:reach(self.ankleX, self.ankleY, self.reachOptions)
end

function Leg:draw(color, layer)
    self.chain:writePoints(self.points)
    if self.tarsus then
        local last = self.chain.count * 2
        self.points[last + 1], self.points[last + 2] = self.footX, self.footY
    end
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

-- Moves the feet in a wave as the creature travels: every leg steps once each `stride` of the distance `travelled`, at the share of the stride its `phase` names, so the steps run along the body in a steady rhythm that keeps pace with any speed, and the feet settle under a creature that stands still. A step takes the `swing` share of the stride at the `speed` of the creature and lands where `land(leg, lead)` says, the foothold `lead` ahead of the rest point of the leg in the direction of travel, as far as the body will carry the foot back behind the rest point before its next step. A foot that falls more than two strides behind, such as after a leap or around a corner, steps at once, and a leg marked `airborne` dangles without stepping.
function Leg.wave(legs, travelled, stride, swing, speed, land)
    if speed < 1 then
        Leg.settle(legs, stride * 0.3, land)
        return
    end
    local time = math.min(0.3, math.max(0.04, swing * stride / speed))
    local lead = stride * (1 + swing) / 2
    for _, leg in ipairs(legs) do
        local cycle = (travelled / stride + leg.phase) % 1
        local previous = leg.cycle or cycle
        leg.cycle = cycle
        if not leg:stepping() and not leg.airborne then
            local dx, dy = leg.footX - leg.restX, leg.footY - leg.restY
            if cycle < previous or dx * dx + dy * dy > stride * stride * 4 then
                local x, y = land(leg, lead)
                leg:stepTo(x, y, time)
            end
        end
    end
end

-- Puts the feet of a creature that stands still back under it one at a time: when no leg steps, the foot farthest from its rest point, beyond `slack`, steps to the foothold `land(leg, 0)` gives it.
function Leg.settle(legs, slack, land)
    local farthest, distance = nil, slack * slack
    for _, leg in ipairs(legs) do
        if leg:stepping() then
            return
        end
        local dx, dy = leg.footX - leg.restX, leg.footY - leg.restY
        if not leg.airborne and dx * dx + dy * dy > distance then
            farthest, distance = leg, dx * dx + dy * dy
        end
    end
    if farthest then
        local x, y = land(farthest, 0)
        farthest:stepTo(x, y)
    end
end

return Leg
