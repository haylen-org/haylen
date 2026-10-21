-- Every shape a body can have: boxes, circles, capsules, convex and concave polygons on dynamic bodies, and a segment and a chain on static ones.
local haylen = require('haylen')
local input = require('haylen.input')
local m = require('haylen.math')
local physics2d = require('haylen.physics2d')
local ui = require('haylen.ui')

local Grab = require('categories.physics.grab')
local Look = require('categories.physics.look')
local PhysicsTest = require('categories.physics.physics-test')
local Scenery = require('categories.physics.scenery')

local Bodies = haylen.class('Bodies', PhysicsTest)

Bodies.code = [[
crate:addBox(90, 72, {density = 1, friction = 0.5})
ball:addCircle(40, {restitution = 0.3})
pill:addCapsule(-40, 0, 40, 0, 26)
star:addPolygon(points)  -- A concave outline splits into convex pieces.
ramp:addSegment(600, 130, 330, 290)
hills:addChain(outline, false)]]

Bodies.ground = 330
Bodies.star = {{0, -62}, {17, -20}, {62, -16}, {27, 12}, {38, 57}, {0, 32}, {-38, 57}, {-27, 12}, {-62, -16}, {-17, -20}}
Bodies.pentagon = {{0, -46}, {44, -14}, {27, 38}, {-27, 38}, {-44, -14}}
Bodies.hills = {{-1600, 60}, {-760, 80}, {-660, 130}, {-580, 200}, {-500, 258}, {-420, 300}, {-330, Bodies.ground}}
Bodies.ramp = {600, 130, 330, 290}
Bodies.view = {1300, 610}
Bodies.kinds = {
    box = {material = 'woodCrate'},
    circle = {material = 'red', spin = true},
    capsule = {material = 'blue'},
    polygon = {material = 'stone'},
    concave = {material = 'gold'},
    steel = {material = 'steelCrate'},
    ball = {material = 'yellow', spin = true},
}

function Bodies:enter()
    self:frame{
        code = Bodies.code,
        hint = 'Drag any body with the mouse or a finger. The buttons drop new bodies, and R or the X button starts over.',
        controls = {
            ui.button{id = 'box', text = 'Drop a crate', onClick = function() self:drop('box') end},
            ui.button{id = 'circle', text = 'Drop a ball', onClick = function() self:drop('circle') end},
            ui.button{id = 'capsule', text = 'Drop a capsule', onClick = function() self:drop('capsule') end},
            ui.button{id = 'polygon', text = 'Drop a pentagon', onClick = function() self:drop('polygon') end},
            ui.button{id = 'concave', text = 'Drop a star', onClick = function() self:drop('concave') end},
            ui.checkbox{id = 'debug', text = 'Show the physics shapes', onChange = function(event) self.debug = event.checked end},
            ui.button{id = 'reset', text = 'Start over', onClick = function() self:build() end},
        },
        focus = 'box',
        view = Bodies.view,
    }
    self.camera.position = {0, Bodies.ground + 60 - Bodies.view[2] / 2}
    self.random = m.random(3)
    self.hills = Scenery.newTerrain(Bodies.hills)
    self:build()
end

function Bodies:build()
    self.world = physics2d.newWorld()
    self.grab = Grab(self.world)
    self.bodies = {}

    local ground = self.world:createBody({type = 'static', x = 0, y = Bodies.ground + 30})
    ground:addBox(8000, 60)
    local hills = self.world:createBody({type = 'static'})
    hills:addChain(Bodies.hills, false)
    local ramp = self.world:createBody({type = 'static'})
    ramp:addSegment(table.unpack(Bodies.ramp))
    Look.dress(ramp, 'wood', {thickness = 14})
    local post = self.world:createBody({type = 'static', x = Bodies.ramp[1] - 6, y = (Bodies.ramp[2] + Bodies.ground) / 2 + 6})
    post:addBox(24, Bodies.ground - Bodies.ramp[2] - 12)
    Look.dress(post, 'darkWood')
    self.statics = {post, ramp}

    for index, kind in ipairs({'box', 'circle', 'capsule', 'polygon', 'concave', 'steel', 'ball'}) do
        self:drop(kind, -360 + index * 105, 120)
    end
end

function Bodies:drop(kind, x, y)
    local body = self.world:createBody({x = x or self.random:range(-300, 300), y = y or -260, rotation = self.random:range(0, math.pi)})
    if kind == 'box' or kind == 'steel' then
        body:addBox(90, 72, {density = 1, friction = 0.5})
    elseif kind == 'circle' or kind == 'ball' then
        body:addCircle(40, {restitution = 0.3})
    elseif kind == 'capsule' then
        body:addCapsule(-40, 0, 40, 0, 26)
    elseif kind == 'polygon' then
        body:addPolygon(Bodies.pentagon)
    else
        body:addPolygon(Bodies.star)
    end
    local look = Bodies.kinds[kind]
    Look.dress(body, look.material, {spin = look.spin, outline = kind == 'concave' and Bodies.star or nil})
    self.bodies[#self.bodies + 1] = body
end

function Bodies:update(dt)
    Bodies.super.update(self, dt)
    if input.pressed('reset') then
        self:build()
    end
    self.grab:update(self.pointer, self.camera)
    self:status(string.format('%d bodies on the stage, simulated in %.2f ms a step.', self.world.bodyCount - 4, self:stepTime()))
end

function Bodies:fixedUpdate(step)
    self:simulate(self.world, step)
end

function Bodies:draw(area)
    Scenery.sky(self.camera, Bodies.ground)
    Scenery.ground(Bodies.ground)
    Scenery.drawTerrain(self.hills)
    Look.drawAll(self.statics)
    Look.drawAll(self.bodies, {layer = 1})
    self.grab:draw()
    if self.debug then
        self.world:debugDraw({layer = 20})
    end
end

return Bodies
