-- Joints under heavy loads side by side, the problems on the left and their solutions on the right. A rope of light segments holds a heavy weight: without `limitLength` it stretches like rubber, and with it a slack joint between its ends keeps its length. A heavy ball hangs from a chain: with links nine hundred times lighter than the ball the chain stretches, and with links ten times denser, eight sub-steps and stiffer joints through `constraintHertz` it holds. A dashed line marks where each load hangs at rest, and red marks how far below it the load sags.
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local input = require('haylen.input')
local physics2d = require('haylen.physics2d')
local ui = require('haylen.ui')

local Grab = require('categories.physics.grab')
local Look = require('categories.physics.look')
local PhysicsTest = require('categories.physics.physics-test')
local Scenery = require('categories.physics.scenery')

local JointStability = haylen.class('JointStability', PhysicsTest)

JointStability.code = [[
local world = physics2d.newWorld({subSteps = 8})
physics2d.newRope(world, {from = top, to = bottom, segments = 16, startBody = beam, endBody = weight, limitLength = true})
link:addBox(10, 22, {density = 10})  -- Links closer in mass to the load.
joint.constraintHertz = 240  -- Stiffer than the default of 60.]]

JointStability.top = -270
JointStability.ground = 360
JointStability.ropeLength, JointStability.ropeSegments = 300, 16
JointStability.links, JointStability.linkLength = 12, 22

function JointStability:enter()
    self:frame{
        code = JointStability.code,
        hint = 'Drag the weights and the balls to load the joints harder, or make the loads heavier. R or the X button starts over.',
        controls = {
            ui.label{text = 'Density of the loads', font = 'caption', color = 'textMuted'},
            ui.slider{id = 'density', value = 15, min = 5, max = 100, step = 5, showValue = true, decimals = 0, onChange = function(event)
                self.density = event.value
                self:build()
            end},
            ui.button{id = 'reset', text = 'Start over', onClick = function() self:build() end},
        },
        focus = 'density',
    }
    self.density = 15
    self.rope = {}
    self:build()
end

function JointStability:build()
    self.cases = {}
    local layout = {
        {x = -600, kind = 'rope', limit = false, subSteps = 4, title = 'A rope without a limit'},
        {x = -200, kind = 'chain', subSteps = 4, linkDensity = 1, title = 'Light links'},
        {x = 200, kind = 'rope', limit = true, subSteps = 4, title = 'A rope with a length limit'},
        {x = 600, kind = 'chain', subSteps = 8, linkDensity = 10, constraintHertz = 240, title = 'Denser and stiffer links'},
    }
    for _, case in ipairs(layout) do
        case.top = JointStability.top
        case.world = physics2d.newWorld({subSteps = case.subSteps})
        case.grab = Grab(case.world)
        case.anchor = case.world:createBody({type = 'static', x = case.x, y = case.top})
        case.anchor:addBox(80, 14)
        if case.kind == 'rope' then
            self:buildRope(case)
        else
            self:buildChain(case)
        end
        self.cases[#self.cases + 1] = case
    end
end

function JointStability:buildRope(case)
    local bottom = case.top + JointStability.ropeLength
    case.load = case.world:createBody({x = case.x, y = bottom + 30})
    case.load:addBox(64, 60, {density = self.density})
    Look.dress(case.load, 'iron')
    case.rope = physics2d.newRope(case.world, {from = {case.x, case.top + 7}, to = {case.x, bottom}, segments = JointStability.ropeSegments, thickness = 6, startBody = case.anchor, endBody = case.load, limitLength = case.limit})
    case.rest = bottom + 30
end

function JointStability:buildChain(case)
    local previous = case.anchor
    local length = JointStability.linkLength
    case.links = {}
    for index = 1, JointStability.links do
        local y = case.top + 7 + (index - 0.5) * length
        local link = case.world:createBody({x = case.x, y = y})
        link:addBox(10, length, {density = case.linkDensity})
        Look.dress(link, 'steel')
        self:stiffen(case, case.world:createJoint('revolute', previous, link, {ax = case.x, ay = y - length / 2}))
        case.links[index] = link
        previous = link
    end
    local bottom = case.top + 7 + JointStability.links * length
    case.load = case.world:createBody({x = case.x, y = bottom + 40})
    case.load:addCircle(40, {density = self.density})
    Look.dress(case.load, 'iron')
    self:stiffen(case, case.world:createJoint('revolute', previous, case.load, {ax = case.x, ay = bottom}))
    case.rest = bottom + 40
end

function JointStability:stiffen(case, joint)
    if case.constraintHertz then
        joint.constraintHertz = case.constraintHertz
    end
end

-- How far the load hangs below its place at rest, against the length of what holds it.
function JointStability:stretch(case)
    local dx, dy = case.load.x - case.x, case.load.y - case.top
    return math.sqrt(dx * dx + dy * dy) / (case.rest - case.top) - 1
end

function JointStability:update(dt)
    JointStability.super.update(self, dt)
    if input.pressed('reset') then
        self:build()
    end
    for _, case in ipairs(self.cases) do
        case.grab:update(self.pointer, self.camera)
    end
    local cases = self.cases
    self:status(string.format('The ropes stretch by %d%% and %d%%, and the chains by %d%% and %d%%, under loads of %d kilograms.', math.floor(self:stretch(cases[1]) * 100 + 0.5), math.floor(self:stretch(cases[3]) * 100 + 0.5), math.floor(self:stretch(cases[2]) * 100 + 0.5), math.floor(self:stretch(cases[4]) * 100 + 0.5), math.floor(cases[1].load.mass + 0.5)))
end

function JointStability:fixedUpdate(step)
    for _, case in ipairs(self.cases) do
        self:simulate(case.world, step)
    end
end

function JointStability:drawCase(case)
    local unit = Look.pixel
    self:label(case.title, case.x, case.top - 38, {size = 19})
    Look.block(case.x, case.top, 80, 14, 'darkWood', 1)
    if case.rope then
        local rope = self.rope
        local count = 0
        for _, point in ipairs(case.rope:points()) do
            rope[count * 2 + 1], rope[count * 2 + 2] = point.x, point.y
            count = count + 1
        end
        for extra = #rope, count * 2 + 1, -1 do
            rope[extra] = nil
        end
        Look.rope(rope, 10, 1)
    else
        Look.drawAll(case.links, {layer = 1})
    end
    Look.draw(case.load, {layer = 2})

    -- The dashed line is the place of the load at rest, and the red bar how far below it the load sags.
    Look.dashed(case.x - 80, case.rest, case.x + 80, case.rest, '#CC2F3352', 3)
    local sag = case.load.y - case.rest
    if sag > 2 then
        graphics2d.drawShape({case.x + 74, case.rest, 10 * unit, sag}, {radius = 5 * unit, color = '#FFF0473E', borderWidth = 2 * unit, borderColor = Look.ink, layer = 3})
    end
    self:label(string.format('%d%% longer', math.max(0, math.floor(self:stretch(case) * 100 + 0.5))), case.x, math.min(math.max(case.load.y, case.rest) + 78, JointStability.ground - 30), {size = 18, plate = sag > 6 and PhysicsTest.problemPlate or PhysicsTest.solutionPlate})
    case.grab:draw()
end

function JointStability:draw(area)
    Scenery.sky(self.camera, JointStability.ground)
    Scenery.ground(JointStability.ground)
    self:compare('The problems', 'Their solutions', 0, -358)
    Look.block(0, JointStability.top - 12, 1620, 16, 'darkWood', 0)
    for _, x in ipairs({-795, 795}) do
        Look.block(x, (JointStability.top + JointStability.ground) / 2 - 10, 22, JointStability.ground - JointStability.top + 20, 'darkWood', 0)
    end
    for _, case in ipairs(self.cases) do
        self:drawCase(case)
    end
end

return JointStability
