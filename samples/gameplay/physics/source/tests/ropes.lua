-- Ropes built by physics2d.newRope: a lamp on a pinned rope, a loose end, a tightrope pinned at both ends, a chain of planks holding a crate and a bola between two balls.
local haylen = require('haylen')
local graphics2d = require('haylen.graphics2d')
local input = require('haylen.input')
local physics2d = require('haylen.physics2d')
local ui = require('haylen.ui')

local Grab = require('grab')
local parts = require('parts')
local sample = require('sample')

local Ropes = haylen.class('Ropes', sample.Test)

local kRopeColor = '#FFBCAAA4'
local kPlankColor = '#FF8D6E63'

function Ropes:enter()
    Ropes.super.enter(self, {
        hint = 'Drag the lamp, the crate or the bola to swing the ropes. Cut snaps every rope in the middle, R or X ties them again.',
        controls = {
            ui.button{id = 'cut', text = 'Cut the ropes', onClick = function() self:cut() end},
            ui.button{id = 'reset', text = 'Tie them again', onClick = function() self:build() end},
        },
        stats = true,
        focus = 'cut',
    })
    self:build()
end

function Ropes:build()
    self.world = physics2d.newWorld()
    self.grab = Grab.new(self.world)
    self.bodies = {}

    local ground = self.world:createBody({type = 'static', x = 0, y = 410})
    parts.box(ground, 1600, 40)
    local beam = self.world:createBody({type = 'static', x = 0, y = -400})
    parts.box(beam, 1500, 24)
    self.statics = {ground, beam}

    local lamp = self:body(-560, -60, function(body) parts.circle(body, 30, {density = 3}) end)
    local crate = self:body(300, 40, function(body) parts.box(body, 90, 70, {density = 2}) end)
    local left = self:body(480, 360, function(body) parts.circle(body, 26) end)
    local right = self:body(700, 360, function(body) parts.circle(body, 26) end)
    self:body(-80, -100, function(body) parts.circle(body, 34, {restitution = 0.2}) end)

    self.ropes = {
        {rope = physics2d.newRope(self.world, {from = {-560, -388}, to = {-560, -60}, segments = 16, thickness = 6, pinStart = true, endBody = lamp}), width = 6},
        {rope = physics2d.newRope(self.world, {from = {-380, -388}, to = {-380, -40}, segments = 18, thickness = 6, pinStart = true}), width = 6},
        {rope = physics2d.newRope(self.world, {from = {-260, 200}, to = {100, 200}, segments = 24, thickness = 8, pinStart = true, pinEnd = true}), width = 8},
        {rope = physics2d.newRope(self.world, {from = {300, -388}, to = {300, 5}, segments = 10, thickness = 14, planks = true, pinStart = true, endBody = crate}), width = 14, color = kPlankColor},
        {rope = physics2d.newRope(self.world, {from = {480, 360}, to = {700, 360}, segments = 12, thickness = 5, startBody = left, endBody = right}), width = 5},
    }
    lamp:applyImpulse(lamp.mass * 500, 0)
end

function Ropes:body(x, y, build)
    local body = self.world:createBody({x = x, y = y})
    build(body)
    self.bodies[#self.bodies + 1] = body
    return body
end

-- Destroys the joint in the middle of every rope that is still whole.
function Ropes:cut()
    for _, entry in ipairs(self.ropes) do
        if entry.rope.valid and not entry.cut then
            local joints = entry.rope:joints()
            joints[#joints // 2 + 1]:destroy()
            entry.cut = true
        end
    end
end

function Ropes:exit()
    Ropes.super.exit(self)
    self.world, self.grab, self.bodies, self.ropes = nil, nil, nil, nil
end

function Ropes:update(dt)
    Ropes.super.update(self, dt)
    if input.pressed('reset') then
        self:build()
    end
    self.grab:update(self.pointer, self.camera)
    self:showStats(string.format('ropes %d\nbodies %d\nstep %.2f ms', #self.ropes, self.world.bodyCount, sample.milliseconds('physics step')))
end

function Ropes:fixedUpdate(step)
    sample.step(self.world, step)
end

-- Every segment draws on its own, so a cut rope shows its gap.
function Ropes:drawRope(entry)
    local color = entry.color or kRopeColor
    for _, segment in ipairs(entry.rope:segments()) do
        local cos, sin = math.cos(segment.rotation), math.sin(segment.rotation)
        local hx, hy = cos * segment.length / 2, sin * segment.length / 2
        graphics2d.drawLine(segment.x - hx, segment.y - hy, segment.x + hx, segment.y + hy, entry.width, color, {layer = 1})
        if not entry.color then
            graphics2d.drawCircle(segment.x + hx, segment.y + hy, entry.width / 2, color, {layer = 1})
        end
    end
end

function Ropes:render()
    self:beginWorld()
    parts.drawAll(self.statics)
    for _, entry in ipairs(self.ropes) do
        self:drawRope(entry)
    end
    parts.drawAll(self.bodies, {layer = 2})
    self.grab:draw()
end

return Ropes
