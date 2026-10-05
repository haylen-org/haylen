-- Every shape a body can have: boxes, circles, capsules, convex and concave polygons on dynamic bodies, and a segment and a chain on static ones.
local haylen = require('haylen')
local input = require('haylen.input')
local m = require('haylen.math')
local physics2d = require('haylen.physics2d')
local ui = require('haylen.ui')

local Grab = require('categories.physics.grab')
local parts = require('categories.physics.parts')
local PhysicsTest = require('categories.physics.physics-test')

local Bodies = haylen.class('Bodies', PhysicsTest)

local kStar = {{0, -46}, {13, -15}, {46, -12}, {20, 9}, {28, 42}, {0, 24}, {-28, 42}, {-20, 9}, {-46, -12}, {-13, -15}}
local kPentagon = {{0, -34}, {32, -10}, {20, 28}, {-20, 28}, {-32, -10}}

function Bodies:enter()
    self:frame{
        hint = 'Drag any body with the mouse or a finger. The buttons drop new bodies, and R or the X button starts over.',
        controls = {
            ui.button{id = 'box', text = 'Drop a box', onClick = function() self:drop('box') end},
            ui.button{id = 'circle', text = 'Drop a circle', onClick = function() self:drop('circle') end},
            ui.button{id = 'capsule', text = 'Drop a capsule', onClick = function() self:drop('capsule') end},
            ui.button{id = 'polygon', text = 'Drop a pentagon', onClick = function() self:drop('polygon') end},
            ui.button{id = 'concave', text = 'Drop a star', onClick = function() self:drop('concave') end},
            ui.checkbox{id = 'debug', text = 'Debug outlines', onChange = function(event) self.debug = event.checked end},
            ui.button{id = 'reset', text = 'Start over', onClick = function() self:build() end},
        },
        focus = 'box',
    }
    self.random = m.random(3)
    self:build()
end

function Bodies:build()
    self.world = physics2d.newWorld()
    self.grab = Grab(self.world)
    self.bodies = {}

    local ground = self.world:createBody({type = 'static', x = 0, y = 410})
    parts.box(ground, 1600, 40)
    local hills = self.world:createBody({type = 'static'})
    local outline = {{-800, 390}, {-800, 200}, {-700, 230}, {-620, 300}, {-520, 330}, {-420, 390}}
    parts.chain(hills, outline, false)
    parts.outline(hills, outline)
    local ramp = self.world:createBody({type = 'static'})
    parts.segment(ramp, 420, 120, 780, 300)
    self.statics = {ground, hills, ramp}

    for index, kind in ipairs({'box', 'circle', 'capsule', 'polygon', 'concave', 'box', 'circle'}) do
        self:drop(kind, -450 + index * 120, 250)
    end
end

function Bodies:drop(kind, x, y)
    local body = self.world:createBody({x = x or self.random:range(-500, 500), y = y or -380, rotation = self.random:range(0, math.pi)})
    if kind == 'box' then
        parts.box(body, 70, 50, {density = 1, friction = 0.5})
    elseif kind == 'circle' then
        parts.circle(body, 30, {restitution = 0.3})
    elseif kind == 'capsule' then
        parts.capsule(body, -30, 0, 30, 0, 20)
    elseif kind == 'polygon' then
        parts.polygon(body, kPentagon)
    else
        parts.polygon(body, kStar)
    end
    self.bodies[#self.bodies + 1] = body
end

function Bodies:update(dt)
    Bodies.super.update(self, dt)
    if input.pressed('reset') then
        self:build()
    end
    self.grab:update(self.pointer, self.camera)
    self:status(string.format('Bodies %d, step %.2f ms', self.world.bodyCount, self:stepTime()))
end

function Bodies:fixedUpdate(step)
    self:simulate(self.world, step)
end

function Bodies:draw(area)
    parts.drawAll(self.statics)
    parts.drawAll(self.bodies)
    self.grab:draw()
    if self.debug then
        self.world:debugDraw({layer = 20})
    end
end

return Bodies
