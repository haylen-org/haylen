-- A bridge of planks from `physics2d.newBridge` between two cliffs, loaded with crates, barrels and a heavy block.
local haylen = require('haylen')
local graphics2d = require('haylen.graphics2d')
local input = require('haylen.input')
local m = require('haylen.math')
local physics2d = require('haylen.physics2d')
local ui = require('haylen.ui')

local Grab = require('grab')
local parts = require('parts')
local sample = require('sample')

local Bridge = haylen.class('Bridge', sample.Test)

local kPlankColor = '#FFA1887F'
local kThickness = 16

function Bridge:enter()
    Bridge.super.enter(self, {
        hint = 'Drop loads on the bridge and drag them along it. R or X rebuilds the bridge.',
        controls = {
            ui.button{id = 'crate', text = 'Drop a crate', onClick = function() self:drop('crate') end},
            ui.button{id = 'barrel', text = 'Drop a barrel', onClick = function() self:drop('barrel') end},
            ui.button{id = 'block', text = 'Drop a heavy block', onClick = function() self:drop('block') end},
            ui.button{id = 'reset', text = 'Rebuild', onClick = function() self:build() end},
        },
        stats = true,
        focus = 'crate',
    })
    self.random = m.random(11)
    self:build()
end

function Bridge:build()
    self.world = physics2d.newWorld()
    self.grab = Grab.new(self.world)
    self.loads = {}

    local left = self.world:createBody({type = 'static'})
    parts.polygon(left, {{-800, 0}, {-560, 0}, {-520, 60}, {-600, 430}, {-800, 430}})
    local right = self.world:createBody({type = 'static'})
    parts.polygon(right, {{560, 0}, {800, 0}, {800, 430}, {600, 430}, {520, 60}})
    self.statics = {left, right}
    self.bridge = physics2d.newBridge(self.world, {from = {-560, -2}, to = {560, -2}, segments = 22, thickness = kThickness, density = 2, friction = 0.8})

    for index = 1, 3 do
        self:drop('crate', -300 + index * 150, -300)
    end
end

function Bridge:drop(kind, x, y)
    local body = self.world:createBody({x = x or self.random:range(-400, 400), y = y or -400, rotation = self.random:range(-0.3, 0.3)})
    if kind == 'crate' then
        parts.box(body, 70, 70)
    elseif kind == 'barrel' then
        parts.circle(body, 32)
    else
        parts.box(body, 140, 80, {density = 6})
        parts.paint(body, '#FF78909C')
    end
    self.loads[#self.loads + 1] = body
end

function Bridge:exit()
    Bridge.super.exit(self)
    self.world, self.grab, self.loads, self.statics, self.bridge = nil, nil, nil, nil, nil
end

function Bridge:update(dt)
    Bridge.super.update(self, dt)
    if input.pressed('reset') then
        self:build()
    end
    self.grab:update(self.pointer, self.camera)
    self:showStats(string.format('planks %d\njoints %d\nloads %d\nstep %.2f ms', #self.bridge:bodies(), #self.bridge:joints(), #self.loads, sample.milliseconds('physics step')))
end

function Bridge:fixedUpdate(step)
    sample.step(self.world, step)
end

function Bridge:render()
    self:beginWorld()
    parts.drawAll(self.statics)
    local points = self.bridge:points()
    for index = 2, #points do
        graphics2d.drawLine(points[index - 1].x, points[index - 1].y - kThickness, points[index].x, points[index].y - kThickness, 3, '#FFBCAAA4')
    end
    for _, segment in ipairs(self.bridge:segments()) do
        local cos, sin = math.cos(segment.rotation), math.sin(segment.rotation)
        local hx, hy = cos * (segment.length / 2 - 1), sin * (segment.length / 2 - 1)
        graphics2d.drawLine(segment.x - hx, segment.y - hy, segment.x + hx, segment.y + hy, kThickness, kPlankColor, {layer = 1})
    end
    parts.drawAll(self.loads, {layer = 2})
    self.grab:draw()
end

return Bridge
