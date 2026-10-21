-- Ropes built by `physics2d.newRope` under a wooden frame: a lamp on a pinned rope, a loose end, a tightrope pinned at both ends between two posts, a chain of planks holding a crate and a bola of two balls on the ground.
local graphics = require('haylen.graphics')
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local input = require('haylen.input')
local physics2d = require('haylen.physics2d')
local ui = require('haylen.ui')

local Grab = require('categories.physics.grab')
local Look = require('categories.physics.look')
local PhysicsTest = require('categories.physics.physics-test')
local Scenery = require('categories.physics.scenery')

local Ropes = haylen.class('Ropes', PhysicsTest)

Ropes.code = [[
physics2d.newRope(world, {from = top, to = lampSpot, segments = 14, pinStart = true, endBody = lamp})
physics2d.newRope(world, {from = top, to = {-380, -40}, segments = 16, pinStart = true})
physics2d.newRope(world, {from = leftPost, to = rightPost, segments = 24, pinStart = true, pinEnd = true})
physics2d.newRope(world, {from = top, to = crateTop, segments = 10, planks = true, pinStart = true, endBody = crate})
physics2d.newRope(world, {from = {470, 300}, to = {690, 300}, startBody = left, endBody = right})
rope:joints()[7]:destroy()  -- Cuts the rope in the middle.]]

Ropes.ground = 330
Ropes.beam = -330
Ropes.captions = {{-560, Ropes.beam + 46}, {-380, Ropes.beam + 46}, {-100, 262}, {300, 120}, {580, 248}}

function Ropes:enter()
    self:frame{
        code = Ropes.code,
        hint = 'Drag the lamp, the crate, the ball or the bola to swing the ropes. Cut snaps every rope in the middle, R or the X button ties them again.',
        controls = {
            ui.button{id = 'swing', text = 'Swing the lamp', onClick = function() self:swing() end},
            ui.button{id = 'throw', text = 'Throw the bola', onClick = function() self:throw() end},
            ui.button{id = 'cut', text = 'Cut the ropes', onClick = function() self:cut() end},
            ui.button{id = 'reset', text = 'Tie them again', onClick = function() self:build() end},
        },
        focus = 'swing',
    }
    local glow = graphics.newImage(128, 128)
    glow:fillGradient({kind = 'radial', stops = {{0, '#FFFFE9A0'}, {0.4, '#80FFD060'}, {1, '#00FFB040'}}})
    self.glow = graphics.newTexture(glow, {filter = 'linear'})
    self:build()
end

function Ropes:build()
    self.world = physics2d.newWorld()
    self.grab = Grab(self.world)
    self.bodies = {}

    local ground = self.world:createBody({type = 'static', x = 0, y = Ropes.ground + 30})
    ground:addBox(8000, 60)
    self.statics = {}
    self:static(0, Ropes.beam, 1520, 26)
    for _, x in ipairs({-740, 740}) do
        self:static(x, (Ropes.beam + Ropes.ground) / 2 + 13, 30, Ropes.ground - Ropes.beam - 26)
    end
    for _, x in ipairs({-300, 100}) do
        self:static(x, (170 + Ropes.ground) / 2, 22, Ropes.ground - 170)
    end

    self.lamp = self:body(-560, -60, function(body) body:addCircle(28, {density = 3}) end)
    local crate = self:body(300, 35, function(body) body:addBox(90, 70, {density = 2}) end, 'woodCrate')
    local left = self:body(470, 300, function(body) body:addCircle(26) end, 'red')
    local right = self:body(690, 300, function(body) body:addCircle(26) end, 'red')
    self.bola = {left, right}
    self:body(-100, -100, function(body) body:addCircle(34, {restitution = 0.2}) end, 'blue')

    self.ropes = {
        {rope = physics2d.newRope(self.world, {from = {-560, Ropes.beam + 12}, to = {-560, -60}, segments = 14, thickness = 6, pinStart = true, endBody = self.lamp}), width = 10, caption = 'A lamp'},
        {rope = physics2d.newRope(self.world, {from = {-380, Ropes.beam + 12}, to = {-380, -40}, segments = 16, thickness = 6, pinStart = true}), width = 10, caption = 'A loose end'},
        {rope = physics2d.newRope(self.world, {from = {-300, 170}, to = {100, 170}, segments = 24, thickness = 8, pinStart = true, pinEnd = true}), width = 12, caption = 'A tightrope'},
        {rope = physics2d.newRope(self.world, {from = {300, Ropes.beam + 12}, to = {300, 0}, segments = 10, thickness = 14, planks = true, pinStart = true, endBody = crate}), width = 14, planks = true, caption = 'Planks'},
        {rope = physics2d.newRope(self.world, {from = {470, 300}, to = {690, 300}, segments = 12, thickness = 5, startBody = left, endBody = right}), width = 9, caption = 'A bola'},
    }
    for _, entry in ipairs(self.ropes) do
        entry.joints = #entry.rope:joints()
        entry.points = {}
    end
    self:swing()
end

function Ropes:static(x, y, width, height)
    local body = self.world:createBody({type = 'static', x = x, y = y})
    body:addBox(width, height)
    Look.dress(body, 'darkWood')
    self.statics[#self.statics + 1] = body
end

function Ropes:body(x, y, build, material)
    local body = self.world:createBody({x = x, y = y})
    build(body)
    if material then
        Look.dress(body, material)
        self.bodies[#self.bodies + 1] = body
    end
    return body
end

function Ropes:swing()
    self.lamp:applyImpulse(self.lamp.mass * 500, 0)
end

function Ropes:throw()
    for index, ball in ipairs(self.bola) do
        ball:applyImpulse(ball.mass * (index == 1 and -500 or 700), -ball.mass * 900)
    end
end

-- Destroys the joint in the middle of every rope that is still whole.
function Ropes:cut()
    for _, entry in ipairs(self.ropes) do
        if entry.rope.valid and not entry.cut then
            local joints = entry.rope:joints()
            entry.cut = #joints // 2 + 1
            joints[entry.cut]:destroy()
        end
    end
end

function Ropes:update(dt)
    Ropes.super.update(self, dt)
    if input.pressed('reset') then
        self:build()
    end
    self.grab:update(self.pointer, self.camera)
    local whole = 0
    for _, entry in ipairs(self.ropes) do
        whole = whole + (entry.cut and 0 or 1)
    end
    self:status(string.format('%d of %d ropes whole, made of %d bodies in all.', whole, #self.ropes, self.world.bodyCount))
end

function Ropes:fixedUpdate(step)
    self:simulate(self.world, step)
end

-- A rope draws as one braid through the ends of its segments, in two pieces once it is cut, and a rope of planks draws every plank.
function Ropes:drawRope(entry)
    local segments = entry.rope:segments()
    if entry.planks then
        for _, segment in ipairs(segments) do
            Look.block(segment.x, segment.y, segment.length + 2, entry.width, 'wood', 1, segment.rotation)
        end
        return
    end
    local points, count = entry.points, 0
    local split = entry.cut and entry.cut - 1 or -1
    for index, segment in ipairs(segments) do
        local hx, hy = math.cos(segment.rotation) * segment.length / 2, math.sin(segment.rotation) * segment.length / 2
        if count == 0 then
            points[1], points[2] = segment.x - hx, segment.y - hy
            count = 1
        end
        points[count * 2 + 1], points[count * 2 + 2] = segment.x + hx, segment.y + hy
        count = count + 1
        if index == split or index == #segments then
            for extra = #points, count * 2 + 1, -1 do
                points[extra] = nil
            end
            Look.rope(points, entry.width, 1)
            count = 0
        end
    end
end

function Ropes:drawLamp()
    local lamp = self.lamp
    local points = self.ropes[1].points
    local upX, upY = 0, -1
    if not self.ropes[1].cut and #points >= 4 then
        local dx, dy = points[#points - 3] - lamp.x, points[#points - 2] - lamp.y
        local length = math.max(math.sqrt(dx * dx + dy * dy), 1)
        upX, upY = dx / length, dy / length
    end
    local angle = math.atan(upY, upX) + math.pi / 2
    local unit = Look.pixel
    graphics2d.draw(self.glow, lamp.x - upX * 40, lamp.y - upY * 40, {width = 190, height = 190, blend = 'additive', layer = 1})
    graphics2d.drawCircle(lamp.x - upX * 6, lamp.y - upY * 6, 15, '#FFFFF4C0', {layer = 2})
    graphics2d.drawShape({lamp.x - 34, lamp.y - 34, 68, 68}, {kind = 'ellipse', startAngle = math.pi, sweep = math.pi, rotation = angle, color = '#FF2FB3C4', borderWidth = 3.5 * unit, borderColor = Look.ink, layer = 2})
    Look.pin(lamp.x + upX * 30, lamp.y + upY * 30, 6 * unit, 2)
end

function Ropes:draw(area)
    Scenery.sky(self.camera, Ropes.ground)
    Scenery.ground(Ropes.ground)
    Look.drawAll(self.statics)
    for _, entry in ipairs(self.ropes) do
        if entry.rope.valid or entry.cut then
            self:drawRope(entry)
        end
    end
    self:drawLamp()
    Look.drawAll(self.bodies, {layer = 2})
    for index, entry in ipairs(self.ropes) do
        local spot = Ropes.captions[index]
        self:label(entry.caption, spot[1], spot[2], {size = 18})
    end
    self.grab:draw()
end

return Ropes
