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
    label(contact.speed, contact.force)  -- How fast they met and how hard they press.
end
-- The weight of the crate counts only the contacts that hold it from below.
for _, contact in ipairs(crate:contacts()) do weight = weight + (contact.normalY > 0.5 and contact.force or 0) end]]

-- Impacts slower than this many units per second only push, so the marks show the landings and not the settling.
Collisions.slowest = 40
Collisions.marks = 8
Collisions.markSeconds = 2.5
Collisions.normalLength = 48
Collisions.velocityScale = 0.12
Collisions.ground = 390
Collisions.wall = 780
Collisions.ramp = {{-780, 390}, {-380, 390}, {-780, 170}}
Collisions.block = {{280, 270}, {440, 270}, {440, 390}, {280, 390}}
-- The values written on the stage are this many pixels of the screen tall, and a value that would cover another moves up until it is clear.
Collisions.tagSize = 18
Collisions.tagTries = 6
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
    self.blockShape = Look.newShape(Collisions.block, 'stone')
    self.placed = {}
    self:build()
end

function Collisions:build()
    if self.world then
        self.world:destroy()
    end
    self.world = physics2d.newWorld({owner = self})
    self.grab = Grab(self.world)
    self:fixture('the ground'):addBox(4000, 40, {offsetY = Collisions.ground + 20})
    local walls = self:fixture('a wall')
    walls:addBox(20, 1000, {offsetX = -Collisions.wall - 10})
    walls:addBox(20, 1000, {offsetX = Collisions.wall + 10})
    self:fixture('the ramp'):addPolygon(Collisions.ramp)
    self:fixture('the stone block'):addPolygon(Collisions.block)
    self.bodies = {}
    self.impacts = {}
    self.last, self.shownImpact = nil, nil
    self:set('status', {text = 'Throw a ball or drop a crate to see its impact.'})
    self.time, self.clock, self.thrown = 0, 0, 0
    self.world.onContactBegin = function(a, b, contact) self:record(a, b, contact) end
    for index, x in ipairs({-180, 60, 600}) do
        self:addCrate(x, Collisions.ground - 30 - index * 5, 50 + index * 10)
    end
end

-- Returns a static body of the room with the name the status gives it.
function Collisions:fixture(name)
    local body = self.world:createBody({type = 'static'})
    body.data = {name = name}
    return body
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

-- Keeps the newest impacts, each with what its contact table told, seen from the body that moved into the other: the body that is not part of the room, or the upper one of two that move. The normal points from the other body toward it, out into the open, and the velocity is the one it kept against the other.
function Collisions:record(a, b, contact)
    if contact.speed < Collisions.slowest then
        return
    end
    local normalX, normalY = contact.normalX, contact.normalY
    local velocityX, velocityY = contact.relativeVelocityX, contact.relativeVelocityY
    local mover, other = b, a
    if b.type == 'static' or (a.type ~= 'static' and normalY > 0.5) then
        mover, other = a, b
        normalX, normalY, velocityX, velocityY = -normalX, -normalY, -velocityX, -velocityY
    end
    local points = {}
    for index, point in ipairs(contact.points) do
        points[index] = {point.x, point.y}
    end
    local impact = {
        x = contact.x, y = contact.y, normalX = normalX, normalY = normalY, velocityX = velocityX, velocityY = velocityY,
        speed = contact.speed, force = contact.force, weight = mover.mass * self.world.gravity.y, points = points, time = self.time,
        mover = mover.data.name, other = other.data.name,
    }
    table.insert(self.impacts, 1, impact)
    self.impacts[Collisions.marks + 1] = nil
    self.last = impact
end

-- The side the moving body came from, where the normal points.
function Collisions.side(impact)
    if math.abs(impact.normalY) >= math.abs(impact.normalX) then
        return impact.normalY < 0 and 'from above' or 'from below'
    end
    return impact.normalX < 0 and 'from the left' or 'from the right'
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

    -- The status tells the last impact in the frame its marks appear, so it always reads the speed written beside them.
    local last = self.last
    if last ~= self.shownImpact then
        self.shownImpact = last
        local points = #last.points == 1 and 'point' or 'points'
        local kept = PhysicsTest.round(math.sqrt(last.velocityX ^ 2 + last.velocityY ^ 2))
        self:set('status', {text = string.format('The last impact: %s hit %s at %d units per second %s, at %d %s, pressed on it with %.1f times its weight and kept %d units per second.', last.mover, last.other, PhysicsTest.round(last.speed), Collisions.side(last), #last.points, points, last.force / last.weight, kept)})
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

-- Draws a value on the stage where it covers none of the values placed before it in this frame, moving up from `x`, `y` as far as it must.
function Collisions:place(text, x, y, color)
    local unit = Look.pixel
    local height = Collisions.tagSize * 1.3 * unit
    local half = #text * Collisions.tagSize * 0.33 * unit + 4 * unit
    local placed = self.placed
    for _ = 1, Collisions.tagTries do
        local clear = true
        for index = 1, #placed, 3 do
            if math.abs(placed[index] - x) < placed[index + 2] + half and math.abs(placed[index + 1] - y) < height then
                clear = false
                break
            end
        end
        if clear then
            break
        end
        y = y - height
    end
    placed[#placed + 1], placed[#placed + 2], placed[#placed + 3] = x, y, half
    self:tag(text, x, y, {size = Collisions.tagSize, color = color, layer = 32})
end

-- The legend names the colors of the marks in the corner of the room, inside its wall.
function Collisions:drawLegend()
    local unit = Look.pixel
    local bounds = graphics2d.canvasBounds()
    local x, y = math.max(bounds.x, -Collisions.wall) + 24 * unit, bounds.y + 24 * unit
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
    Look.drawShape(self.blockShape, 0)
    for _, side in ipairs({-1, 1}) do
        Look.block(side * (Collisions.wall + 20), (bounds.y + Collisions.ground) / 2, 40, Collisions.ground - bounds.y + 20, 'stone', -12)
    end
    Look.drawAll(self.bodies, {layer = 1})
    local placed = self.placed
    for index = #placed, 1, -1 do
        placed[index] = nil
    end
    for _, body in ipairs(self.bodies) do
        local force = body.valid and body.data.name == 'a crate' and Collisions.weight(body) or 0
        if force > 0 then
            self:place(tostring(PhysicsTest.round(force)), body.x, body.y)
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
            self:place(tostring(PhysicsTest.round(impact.speed)), impact.x + 30, impact.y - 26, m.color(1, 1, 1, fade))
        end
    end
    self:drawLegend()
    self.grab:draw()
end

return Collisions
