-- Collision report: balls and crates thrown at a floor, a ramp and a block, where every contact that begins marks its points, the normal it pushed along and the velocity the bodies kept, with how fast they met and how hard they press, and every crate at rest shows the force its weight presses the floor with.
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local input = require('haylen.input')
local m = require('haylen.math')
local physics2d = require('haylen.physics2d')
local ui = require('haylen.ui')

local Grab = require('categories.physics.grab')
local parts = require('categories.physics.parts')
local PhysicsTest = require('categories.physics.physics-test')

local Collisions = haylen.class('Collisions', PhysicsTest)

-- Impacts slower than this many units per second only push, so the marks show the landings and not the settling.
Collisions.slowest = 40
Collisions.marks = 8
Collisions.markSeconds = 2.5
Collisions.normalLength = 48
Collisions.velocityScale = 0.12

function Collisions:enter()
    self:frame{
        hint = 'Throw balls and crates, or drag them, and every impact marks its points in white, the normal it pushed along in yellow and the velocity the bodies kept in blue. Each crate shows the force its weight presses down with. R or the X button starts over.',
        controls = {
            ui.button{id = 'ball', text = 'Throw a ball', onClick = function() self:throw('ball') end},
            ui.button{id = 'crate', text = 'Drop a crate', onClick = function() self:throw('crate') end},
            ui.button{id = 'reset', text = 'Start over', onClick = function() self:build() end},
        },
        focus = 'ball',
    }
    self.random = m.random(64)
    self:build()
end

function Collisions:build()
    self.world = physics2d.newWorld()
    self.grab = Grab(self.world)
    local room = self.world:createBody({type = 'static'})
    parts.box(room, 1600, 40, {offsetY = 410})
    parts.box(room, 20, 860, {offsetX = -790})
    parts.box(room, 20, 860, {offsetX = 790})
    parts.polygon(room, {{-780, 390}, {-380, 390}, {-780, 170}})
    parts.box(room, 160, 120, {offsetX = 360, offsetY = 330})
    self.statics = {room}
    self.bodies = {}
    self.impacts = {}
    self.last = nil
    self.time = 0
    self.world.onContactBegin = function(a, b, contact) self:record(a, b, contact) end
end

function Collisions:throw(kind)
    local x = self.random:range(-500, 600)
    local body = self.world:createBody({x = x, y = -360, vx = self.random:range(-300, 300), vy = self.random:range(0, 400), angularVelocity = self.random:range(-3, 3)})
    if kind == 'ball' then
        parts.circle(body, self.random:range(18, 34), {density = 1, restitution = 0.5})
    else
        local size = self.random:range(40, 80)
        parts.box(body, size, size, {density = 2, friction = 0.6})
    end
    body.data.name = kind == 'ball' and 'Ball' or 'Crate'
    self.bodies[#self.bodies + 1] = body
end

-- Keeps the newest impacts, each with what its contact table told.
function Collisions:record(a, b, contact)
    if contact.speed < Collisions.slowest then
        return
    end
    local points = {}
    for index, point in ipairs(contact.points) do
        points[index] = {point.x, point.y, point.separation}
    end
    local impact = {
        x = contact.x, y = contact.y, normalX = contact.normalX, normalY = contact.normalY,
        velocityX = contact.relativeVelocityX, velocityY = contact.relativeVelocityY,
        speed = contact.speed, force = contact.force, points = points, time = self.time,
        first = Collisions.nameOf(a), second = Collisions.nameOf(b),
    }
    table.insert(self.impacts, 1, impact)
    self.impacts[Collisions.marks + 1] = nil
    self.last = impact
end

function Collisions.nameOf(body)
    return body and body.data and body.data.name or 'Room'
end

-- The side of the second body that took the impact, the opposite of where the normal points.
function Collisions.side(impact)
    if math.abs(impact.normalY) >= math.abs(impact.normalX) then
        return impact.normalY > 0 and 'from above' or 'from below'
    end
    return impact.normalX > 0 and 'from the left' or 'from the right'
end

-- The force with which the body presses down on what holds it, from its contacts whose normal points down from it.
function Collisions.weight(body)
    local force = 0
    for _, contact in ipairs(body:contacts()) do
        if contact.normalY > 0.5 then
            force = force + contact.force
        end
    end
    return force
end

function Collisions:update(dt)
    Collisions.super.update(self, dt)
    self.time = self.time + dt
    if input.pressed('reset') then
        self:build()
    end
    self.grab:update(self.pointer, self.camera)
    local last = self.last
    if last then
        self:status(string.format('%s met %s at %.0f units per second, which took it %s, at %d %s, pressing with %.0f and still sliding at %.0f units per second, step %.2f ms', last.first, last.second, last.speed, Collisions.side(last), #last.points, #last.points == 1 and 'point' or 'points', last.force, math.sqrt(last.velocityX ^ 2 + last.velocityY ^ 2), self:stepTime()))
    else
        self:status(string.format('Throw something to see its impact, step %.2f ms', self:stepTime()))
    end
end

function Collisions:fixedUpdate(step)
    self:simulate(self.world, step)
end

function Collisions.arrow(x, y, dx, dy, color)
    local length = math.sqrt(dx * dx + dy * dy)
    if length < 4 then
        return
    end
    local ux, uy = dx / length, dy / length
    local tipX, tipY = x + dx, y + dy
    graphics2d.drawLine(x, y, tipX - ux * 8, tipY - uy * 8, 3, color, {layer = 4})
    graphics2d.drawPolygon({{tipX, tipY}, {tipX - ux * 12 - uy * 6, tipY - uy * 12 + ux * 6}, {tipX - ux * 12 + uy * 6, tipY - uy * 12 - ux * 6}}, color, {layer = 4})
end

function Collisions:draw(area)
    parts.drawAll(self.statics)
    parts.drawAll(self.bodies, {layer = 1})
    for _, body in ipairs(self.bodies) do
        local force = Collisions.weight(body)
        if body.data.name == 'Crate' and force > 0 then
            Collisions.caption(string.format('%.0f', force), body.x, body.y, {size = 15, anchor = {0.5, 0.5}, layer = 5, color = '#FF1B2030'})
        end
    end

    for _, impact in ipairs(self.impacts) do
        local fade = 1 - (self.time - impact.time) / Collisions.markSeconds
        if fade > 0 then
            local alpha = math.floor(fade * 255)
            for _, point in ipairs(impact.points) do
                graphics2d.drawCircle(point[1], point[2], 6, m.color(1, 1, 1, fade), {layer = 4})
            end
            Collisions.arrow(impact.x, impact.y, impact.normalX * Collisions.normalLength, impact.normalY * Collisions.normalLength, string.format('#%02XFFD54F', alpha))
            Collisions.arrow(impact.x, impact.y, impact.velocityX * Collisions.velocityScale, impact.velocityY * Collisions.velocityScale, string.format('#%02X64B5F6', alpha))
            Collisions.caption(string.format('%.0f', impact.speed), impact.x + 10, impact.y - 14, {anchor = {0, 1}, layer = 5, color = m.color(1, 1, 1, fade)})
        end
    end
    self.grab:draw()
end

return Collisions
