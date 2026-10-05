-- Fast bodies side by side in two worlds: on the left they pass through thin walls, moving plates and thin floors, and on the right continuous collision stops them at static walls and floors and `bullet = true` stops them at moving plates.
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local input = require('haylen.input')
local physics2d = require('haylen.physics2d')
local ui = require('haylen.ui')

local parts = require('categories.physics.parts')
local PhysicsTest = require('categories.physics.physics-test')

local Tunneling = haylen.class('Tunneling', PhysicsTest)

local kInterval = 0.6
local kLifetime = 1.5
local kWallX = 250
local kStaticLane, kMovingLane, kFloorY = -260, -40, 330
local kLanes = {'Static wall', 'Moving plate', 'Thin floor'}

function Tunneling:enter()
    self:frame{
        hint = 'Every 0.6 seconds both worlds shoot a ball at a thin static wall, a ball at a thin moving plate and drop a crate onto a thin floor. Raise the speed to see more of them pass on the left. R or the X button starts over.',
        controls = {
            ui.label{text = 'Speed of the shots in units per second', font = 'caption', color = 'textMuted'},
            ui.slider{id = 'speed', value = 6000, min = 1000, max = 12000, step = 500, showValue = true, decimals = 0, onChange = function(event) self.speed = event.value end},
            ui.button{id = 'shoot', text = 'Shoot now', onClick = function() self:shoot() end},
            ui.button{id = 'reset', text = 'Start over', onClick = function() self:build() end},
        },
        pointer = false,
        focus = 'speed',
    }
    self.speed = 6000
    self:build()
end

-- The left world has no continuous collision and fires plain bodies, and the right world keeps it on and fires bullets.
function Tunneling:build()
    self.sides = {}
    for index, fixed in ipairs({false, true}) do
        local origin = index == 1 and -400 or 400
        local world = physics2d.newWorld({continuous = fixed})
        local walls = world:createBody({type = 'static'})
        parts.segment(walls, origin + kWallX, kStaticLane - 70, origin + kWallX, kStaticLane + 70)
        parts.segment(walls, origin - 150, kFloorY, origin + 150, kFloorY)
        local hinge = world:createBody({type = 'static', x = origin + kWallX, y = kMovingLane - 90})
        local plate = world:createBody({x = origin + kWallX, y = kMovingLane})
        parts.box(plate, 4, 160, {density = 20})
        world:createJoint('revolute', hinge, plate, {ax = origin + kWallX, ay = kMovingLane - 80, enableSpring = true, hertz = 2, dampingRatio = 0.5})
        self.sides[index] = {world = world, origin = origin, bullet = fixed, statics = {walls, plate}, shots = {}, through = {0, 0, 0}, stopped = {0, 0, 0}}
    end
    self.clock = 0
end

function Tunneling:shoot()
    for _, side in ipairs(self.sides) do
        local world, origin = side.world, side.origin
        local ball = world:createBody({x = origin - 330, y = kStaticLane, vx = self.speed, gravityScale = 0, bullet = side.bullet})
        parts.circle(ball, 6)
        local fast = world:createBody({x = origin - 330, y = kMovingLane, vx = self.speed, gravityScale = 0, bullet = side.bullet})
        parts.circle(fast, 6)
        local crate = world:createBody({x = origin, y = 80, vy = self.speed, bullet = side.bullet})
        parts.box(crate, 30, 30)
        for lane, body in ipairs({ball, fast, crate}) do
            side.shots[#side.shots + 1] = {body = body, lane = lane, age = 0}
        end
    end
end

-- A shot counts once: as through when it gets past its wall or floor, and as stopped when it comes to rest or its time runs out first.
function Tunneling:count(side, step)
    for index = #side.shots, 1, -1 do
        local shot = side.shots[index]
        shot.age = shot.age + step
        local body = shot.body
        local past = shot.lane == 3 and body.y > kFloorY + 20 or shot.lane ~= 3 and body.x > side.origin + kWallX + 20
        local resting = shot.age > 0.3 and body.velocity:length() < 30
        if past or resting or shot.age > kLifetime then
            local tally = past and side.through or side.stopped
            tally[shot.lane] = tally[shot.lane] + 1
            body:destroy()
            table.remove(side.shots, index)
        end
    end
end

function Tunneling:update(dt)
    Tunneling.super.update(self, dt)
    if input.pressed('reset') then
        self:build()
    end
    local left, right = self.sides[1], self.sides[2]
    self:status(string.format('Through on the left %d, %d, %d and on the right %d, %d, %d, step %.2f ms', left.through[1], left.through[2], left.through[3], right.through[1], right.through[2], right.through[3], self:stepTime()))
end

function Tunneling:fixedUpdate(step)
    self.clock = self.clock + step
    if self.clock >= kInterval then
        self.clock = self.clock - kInterval
        self:shoot()
    end
    for _, side in ipairs(self.sides) do
        self:simulate(side.world, step)
        self:count(side, step)
    end
end

function Tunneling:draw(area)
    graphics2d.drawLine(0, -430, 0, 430, 2, '#44FFFFFF')
    local titles = {'Without continuous collision', 'Continuous collision and bullets'}
    for index, side in ipairs(self.sides) do
        local origin = side.origin
        graphics2d.drawText(nil, titles[index], origin, -400, {size = 28, color = index == 1 and '#FFFF8A84' or '#FF6FDCA0', anchor = {0.5, 0.5}})
        parts.drawAll(side.statics)
        for _, shot in ipairs(side.shots) do
            parts.draw(shot.body, {layer = 1})
        end
        for lane, y in ipairs({kStaticLane - 90, kMovingLane - 110, kFloorY + 40}) do
            local text = string.format('%s: %d through, %d stopped', kLanes[lane], side.through[lane], side.stopped[lane])
            graphics2d.drawText(nil, text, origin - 360, y, {size = 20, color = '#CCFFFFFF', anchor = {0, 0.5}})
        end
    end
end

return Tunneling
