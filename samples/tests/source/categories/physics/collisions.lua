-- Collision report: balls and crates thrown at a floor, a stone ramp and a block, where every contact that begins marks its points, the normal it pushed along and the velocity the bodies kept, with how fast they met and how hard they press, and every crate at rest shows the force its weight presses the floor with. A few crates rest on the floor and bodies keep falling for a while when the test opens.
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local input = require('haylen.input')
local m = require('haylen.math')
local physics2d = require('haylen.physics2d')
local ui = require('haylen.ui')

local Grab = require('categories.physics.grab')
local Look = require('categories.physics.look')
local PhysicsTest = require('categories.physics.physics-test')
local Scenery = require('categories.physics.scenery')

local Collisions = haylen.class('Collisions', PhysicsTest)

Collisions.code = [[
world.onContactBegin = function(a, b, contact)
    mark(contact.points, contact.normalX, contact.normalY, contact.relativeVelocityX, contact.relativeVelocityY)
    print(contact.speed, contact.force)  -- How fast they met and how hard they press.
end
for _, contact in ipairs(crate:contacts()) do if contact.normalY > 0.5 then weight = weight + contact.force end end]]

-- Impacts slower than this many units per second only push, so the marks show the landings and not the settling.
Collisions.slowest = 40
Collisions.marks = 8
Collisions.markSeconds = 2.5
Collisions.normalLength = 48
Collisions.velocityScale = 0.12
Collisions.ground = 390
Collisions.ramp = {{-780, 390}, {-380, 390}, {-780, 170}}
Collisions.block = {360, 330, 160, 120}
Collisions.firstThrows = 8
Collisions.throwEvery = 1.1
Collisions.maxBodies = 16
-- The rows of the legend: what each mark is drawn with and what it shows.
Collisions.leftAnchor = {0, 0.5}
Collisions.legend = {{'#FFFFFFFF', 'Contact points'}, {'#FFFFC93C', 'Normal of the push'}, {'#FF64B5F6', 'Velocity the bodies kept'}}

function Collisions:enter()
    self:frame{
        code = Collisions.code,
        hint = 'Throw balls and crates, or drag them, and every impact marks its points in white, the normal it pushed along in yellow and the velocity the bodies kept in blue. Each crate shows the force its weight presses down with. R or the X button starts over.',
        controls = {
            ui.button{id = 'ball', text = 'Throw a ball', onClick = function() self:throw('ball') end},
            ui.button{id = 'crate', text = 'Drop a crate', onClick = function() self:throw('crate') end},
            ui.button{id = 'reset', text = 'Start over', onClick = function() self:build() end},
        },
        focus = 'ball',
        view = {1500, 660},
    }
    self.camera.position = {0, 90}
    self.random = m.random(64)
    self.rampShape = Look.newShape(Collisions.ramp, 'stone')
    self:build()
end

function Collisions:build()
    self.world = physics2d.newWorld()
    self.grab = Grab(self.world)
    local room = self.world:createBody({type = 'static'})
    room:addBox(4000, 40, {offsetY = Collisions.ground + 20})
    room:addBox(20, 1000, {offsetX = -790})
    room:addBox(20, 1000, {offsetX = 790})
    room:addPolygon(Collisions.ramp)
    local block = Collisions.block
    room:addBox(block[3], block[4], {offsetX = block[1], offsetY = block[2]})
    self.bodies = {}
    self.impacts = {}
    self.last = nil
    self.time, self.clock, self.thrown = 0, 0, 0
    self.world.onContactBegin = function(a, b, contact) self:record(a, b, contact) end
    for index, x in ipairs({-180, 60, 600}) do
        self:addCrate(x, Collisions.ground - 30 - index * 5, 50 + index * 10)
    end
end

function Collisions:addCrate(x, y, size)
    local body = self.world:createBody({x = x, y = y})
    body:addBox(size, size, {density = 2, friction = 0.6})
    Look.dress(body, 'woodCrate')
    body.data.name = 'a crate'
    self:keep(body)
    return body
end

function Collisions:keep(body)
    self.bodies[#self.bodies + 1] = body
    if #self.bodies > Collisions.maxBodies then
        table.remove(self.bodies, 1):destroy()
    end
end

function Collisions:throw(kind)
    local x = self.random:range(-500, 600)
    local vx, vy, spin = self.random:range(-300, 300), self.random:range(0, 400), self.random:range(-3, 3)
    if kind == 'ball' then
        local body = self.world:createBody({x = x, y = -360, vx = vx, vy = vy, angularVelocity = spin})
        body:addCircle(self.random:range(18, 34), {density = 1, restitution = 0.5})
        Look.dress(body, Look.toys[self.random:integer(1, #Look.toys)], {spin = true})
        body.data.name = 'a ball'
        self:keep(body)
    else
        local body = self:addCrate(x, -360, self.random:range(40, 80))
        body.velocity, body.angularVelocity = {vx, vy}, spin
    end
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
    return body and body.data and body.data.name or 'the ground'
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

    -- The first bodies fall by themselves, a ball and a crate in turn.
    self.clock = self.clock + dt
    if self.thrown < Collisions.firstThrows and self.clock > Collisions.throwEvery then
        self.clock, self.thrown = 0, self.thrown + 1
        self:throw(self.thrown % 2 == 1 and 'ball' or 'crate')
    end

    local last = self.last
    if last then
        local points = #last.points == 1 and 'point' or 'points'
        self:status(string.format('The last impact: %s met %s at %d units per second %s, at %d %s, pressing with %d and sliding on at %d units per second.', last.first, last.second, PhysicsTest.round(last.speed), Collisions.side(last), #last.points, points, PhysicsTest.round(last.force), PhysicsTest.round(math.sqrt(last.velocityX ^ 2 + last.velocityY ^ 2))))
    else
        self:status('Throw a ball or drop a crate to see its impact.')
    end
end

function Collisions:fixedUpdate(step)
    self:simulate(self.world, step)
end

-- Draws an arrow with the bold outline from `x`, `y` along `dx`, `dy`.
function Collisions.arrow(x, y, dx, dy, color)
    local length = math.sqrt(dx * dx + dy * dy)
    if length < 4 then
        return
    end
    local ux, uy = dx / length, dy / length
    local tipX, tipY = x + dx, y + dy
    Look.line(x, y, tipX - ux * 10, tipY - uy * 10, 4, color, 30)
    graphics2d.drawPolygon({{tipX, tipY}, {tipX - ux * 16 - uy * 9, tipY - uy * 16 + ux * 9}, {tipX - ux * 16 + uy * 9, tipY - uy * 16 - ux * 9}}, color, {layer = 31})
end

-- The legend names the colors of the marks in a corner of the stage.
function Collisions:drawLegend()
    local unit = Look.pixel
    local bounds = graphics2d.canvasBounds()
    local x, y = bounds.x + 24 * unit, bounds.y + 24 * unit
    local row = 36 * unit
    graphics2d.drawShape({x, y, 330 * unit, row * #Collisions.legend + 56 * unit}, {radius = 12 * unit, color = '#EE2F3352', layer = 40})
    for index, entry in ipairs(Collisions.legend) do
        local cy = y + 22 * unit + (index - 1) * row
        if index == 1 then
            graphics2d.drawCircle(x + 26 * unit, cy, 7 * unit, entry[1], {layer = 41})
        else
            graphics2d.drawLine(x + 12 * unit, cy, x + 40 * unit, cy, 5 * unit, entry[1], {layer = 41})
        end
        self:tag(entry[2], x + 54 * unit, cy, {anchor = Collisions.leftAnchor, size = 20, layer = 41})
    end
    self:tag('Crates show the force of their weight', x + 14 * unit, y + 22 * unit + #Collisions.legend * row, {anchor = Collisions.leftAnchor, size = 18, color = '#FFC9D3E2', layer = 41})
end

function Collisions:draw(area)
    local bounds = graphics2d.canvasBounds()
    Scenery.sky(self.camera, Collisions.ground)
    Scenery.ground(Collisions.ground)
    Look.drawShape(self.rampShape, 0)
    local block = Collisions.block
    Look.block(block[1], block[2], block[3], block[4], 'stone', 0)
    for _, x in ipairs({-800, 800}) do
        Look.block(x, (bounds.y + Collisions.ground) / 2, 40, Collisions.ground - bounds.y + 20, 'stone', -12)
    end
    Look.drawAll(self.bodies, {layer = 1})
    for _, body in ipairs(self.bodies) do
        local force = body.valid and body.data.name == 'a crate' and Collisions.weight(body) or 0
        if force > 0 then
            self:tag(tostring(PhysicsTest.round(force)), body.x, body.y, {size = 18, layer = 32})
        end
    end

    for _, impact in ipairs(self.impacts) do
        local fade = 1 - (self.time - impact.time) / Collisions.markSeconds
        if fade > 0 then
            for _, point in ipairs(impact.points) do
                graphics2d.drawCircle(point[1], point[2], 7, m.color(1, 1, 1, fade), {layer = 31})
            end
            Collisions.arrow(impact.x, impact.y, impact.normalX * Collisions.normalLength, impact.normalY * Collisions.normalLength, m.color('#FFFFC93C'):withAlpha(fade))
            Collisions.arrow(impact.x, impact.y, impact.velocityX * Collisions.velocityScale, impact.velocityY * Collisions.velocityScale, m.color('#FF64B5F6'):withAlpha(fade))
            self:tag(tostring(PhysicsTest.round(impact.speed)), impact.x + 30, impact.y - 26, {size = 16, color = m.color(1, 1, 1, fade), layer = 32})
        end
    end
    self:drawLegend()
    self.grab:draw()
end

return Collisions
