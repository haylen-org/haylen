-- The same moving platforms in two worlds: on the left they jump to their next place every step with `body:setTransform`, so they slide out from under their loads and knock them away, and on the right `body:moveTo` gives them the velocity that reaches that place in one step, so friction carries crates and balls and a mover rides them.
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local input = require('haylen.input')
local physics2d = require('haylen.physics2d')
local ui = require('haylen.ui')

local parts = require('categories.physics.parts')
local PhysicsTest = require('categories.physics.physics-test')

local MovingPlatforms = haylen.class('MovingPlatforms', PhysicsTest)

local kGravity = 1600
local kRadius, kHeight = 14, 56

function MovingPlatforms:enter()
    self:frame{
        hint = 'The platforms on both sides follow the same paths. Drop new loads on them with the button. R or the X button starts over.',
        controls = {
            ui.label{text = 'Speed of the platforms', font = 'caption', color = 'textMuted'},
            ui.slider{id = 'speed', value = 1, min = 0.25, max = 2.5, step = 0.25, showValue = true, decimals = 2, onChange = function(event) self.speed = event.value end},
            ui.button{id = 'drop', text = 'Drop loads', onClick = function() self:drop() end},
            ui.button{id = 'reset', text = 'Start over', onClick = function() self:build() end},
        },
        pointer = false,
        focus = 'speed',
    }
    self.speed = 1
    self:build()
end

function MovingPlatforms:build()
    self.sides = {}
    for index = 1, 2 do
        local origin = index == 1 and -400 or 400
        local world = physics2d.newWorld({gravity = {0, kGravity}})
        local ground = world:createBody({type = 'static', x = origin, y = 400})
        parts.box(ground, 780, 40)
        local side = {world = world, origin = origin, ground = ground, platforms = {}, loads = {}}
        side.platforms[1] = self:platform(side, 'slider', 200, 16)
        side.platforms[2] = self:platform(side, 'lift', 160, 16)
        side.platforms[3] = self:platform(side, 'turner', 240, 16)
        self.sides[index] = side
    end
    self.time = 0
    self:drop()
    local right = self.sides[2]
    self.mover = physics2d.newMover(right.world, {x = right.origin - 150, y = -200, radius = kRadius, height = kHeight, snapDistance = 12})
    self.velocity = {x = 0, y = 0}
end

function MovingPlatforms:platform(side, kind, width, height)
    local body = side.world:createBody({type = 'kinematic'})
    parts.box(body, width, height, {friction = 0.9})
    parts.paint(body, '#FFFFB74D')
    local platform = {body = body, kind = kind}
    self:place(side, platform, 0, false)
    return platform
end

-- The paths of the platforms: one slides from side to side, one rises and sinks, one turns around its middle.
function MovingPlatforms:pathOf(side, kind, time)
    if kind == 'slider' then
        return side.origin - 150 + math.sin(time * 1.3) * 160, -150, 0
    elseif kind == 'lift' then
        return side.origin + 220, 120 + math.sin(time) * 200, 0
    end
    return side.origin - 120, 220, math.sin(time * 0.9) * 0.6
end

function MovingPlatforms:place(side, platform, time, moving)
    local x, y, rotation = self:pathOf(side, platform.kind, time)
    if moving and side == self.sides[2] then
        platform.body:moveTo(x, y, rotation)
    else
        platform.body:setTransform(x, y, rotation)
    end
end

function MovingPlatforms:drop()
    for _, side in ipairs(self.sides) do
        for _, platform in ipairs(side.platforms) do
            local x, y = self:pathOf(side, platform.kind, self.time or 0)
            local crate = side.world:createBody({x = x - 50, y = y - 40})
            parts.box(crate, 44, 44, {friction = 0.8})
            local ball = side.world:createBody({x = x + 50, y = y - 40})
            parts.circle(ball, 18, {friction = 0.8})
            parts.paint(ball, '#FFFFD54F')
            side.loads[#side.loads + 1] = crate
            side.loads[#side.loads + 1] = ball
        end
    end
end

function MovingPlatforms:update(dt)
    MovingPlatforms.super.update(self, dt)
    if input.pressed('reset') then
        self:build()
    end
    local ground = self.mover.groundBody
    self:status(string.format('The mover stands on %s at %.0f units per second, step %.2f ms', ground and ground.type or 'nothing', self.mover.groundVelocity:length(), self:stepTime()))
end

function MovingPlatforms:fixedUpdate(step)
    self.time = self.time + step * self.speed
    for _, side in ipairs(self.sides) do
        for _, platform in ipairs(side.platforms) do
            self:place(side, platform, self.time, true)
        end
    end

    local mover = self.mover
    local velocity = mover:clip(0, self.velocity.y + kGravity * step)
    mover:move(velocity.x * step, velocity.y * step)
    self.velocity = mover:clip(velocity.x, velocity.y)

    for _, side in ipairs(self.sides) do
        self:simulate(side.world, step)
        for index = #side.loads, 1, -1 do
            if side.loads[index].y > 600 then
                table.remove(side.loads, index):destroy()
            end
        end
    end
end

function MovingPlatforms:draw(area)
    graphics2d.drawLine(0, -430, 0, 430, 2, '#44FFFFFF')
    local titles = {'Moved with setTransform', 'Moved with moveTo'}
    for index, side in ipairs(self.sides) do
        graphics2d.drawText(nil, titles[index], side.origin, -390, {size = 28, color = index == 1 and '#FFFF8A84' or '#FF6FDCA0', anchor = {0.5, 0.5}})
        parts.draw(side.ground)
        for _, platform in ipairs(side.platforms) do
            parts.draw(platform.body)
        end
        parts.drawAll(side.loads, {layer = 1})
    end
    local position = self.mover.position
    local reach = kHeight / 2 - kRadius
    graphics2d.drawLine(position.x, position.y - reach, position.x, position.y + reach, kRadius * 2, '#FF6FDCA0', {layer = 2})
    graphics2d.drawCircle(position.x, position.y - reach, kRadius, '#FF6FDCA0', {layer = 2})
    graphics2d.drawCircle(position.x, position.y + reach, kRadius, '#FF6FDCA0', {layer = 2})
end

return MovingPlatforms
