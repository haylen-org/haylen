-- Joints under heavy loads side by side, the problems on the left and their solutions on the right. A rope of light segments holds a heavy weight: without `limitLength` it stretches like rubber, and with it a slack joint between its ends keeps its length. A heavy ball hangs from a chain: with links nine hundred times lighter than the ball the chain stretches, and with links ten times denser, eight sub-steps and stiffer joints through `constraintHertz` it holds.
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local input = require('haylen.input')
local physics2d = require('haylen.physics2d')
local ui = require('haylen.ui')

local Grab = require('categories.physics.grab')
local parts = require('categories.physics.parts')
local PhysicsTest = require('categories.physics.physics-test')

local JointStability = haylen.class('JointStability', PhysicsTest)

local kTop = -330
local kRopeLength, kRopeSegments = 300, 16
local kLinks, kLinkLength = 12, 22

function JointStability:enter()
    self:frame{
        hint = 'Drag the weights and the balls to load the joints harder, or make the loads heavier. The numbers show how long each rope and chain is against its rest length. R or the X button starts over.',
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
    self:build()
end

function JointStability:build()
    self.cases = {}
    local layout = {
        {x = -600, top = kTop, kind = 'rope', limit = false, subSteps = 4, title = 'Rope without a limit'},
        {x = 200, top = kTop, kind = 'rope', limit = true, subSteps = 4, title = 'Rope with "limitLength"'},
        {x = -200, top = kTop, kind = 'chain', subSteps = 4, linkDensity = 1, title = 'Light links'},
        {x = 600, top = kTop, kind = 'chain', subSteps = 8, linkDensity = 10, constraintHertz = 240, title = 'Denser, stiffer, more sub-steps'},
    }
    for _, case in ipairs(layout) do
        case.world = physics2d.newWorld({subSteps = case.subSteps})
        case.grab = Grab(case.world)
        case.anchor = case.world:createBody({type = 'static', x = case.x, y = case.top})
        parts.box(case.anchor, 80, 12)
        if case.kind == 'rope' then
            self:buildRope(case)
        else
            self:buildChain(case)
        end
        self.cases[#self.cases + 1] = case
    end
end

function JointStability:buildRope(case)
    local bottom = case.top + kRopeLength
    case.load = case.world:createBody({x = case.x, y = bottom + 20})
    parts.box(case.load, 50, 40, {density = self.density})
    parts.paint(case.load, '#FF78909C')
    case.rope = physics2d.newRope(case.world, {from = {case.x, case.top + 6}, to = {case.x, bottom}, segments = kRopeSegments, thickness = 6, startBody = case.anchor, endBody = case.load, limitLength = case.limit})
    case.rest = kRopeLength
end

function JointStability:buildChain(case)
    local previous = case.anchor
    case.links = {}
    for index = 1, kLinks do
        local y = case.top + 6 + (index - 0.5) * kLinkLength
        local link = case.world:createBody({x = case.x, y = y})
        parts.box(link, 8, kLinkLength, {density = case.linkDensity})
        parts.paint(link, '#FFBCAAA4')
        self:stiffen(case, case.world:createJoint('revolute', previous, link, {ax = case.x, ay = y - kLinkLength / 2}))
        case.links[index] = link
        previous = link
    end
    local bottom = case.top + 6 + kLinks * kLinkLength
    case.load = case.world:createBody({x = case.x, y = bottom + 36})
    parts.circle(case.load, 36, {density = self.density})
    parts.paint(case.load, '#FF78909C')
    self:stiffen(case, case.world:createJoint('revolute', previous, case.load, {ax = case.x, ay = bottom}))
    case.rest = kLinks * kLinkLength + 36
end

function JointStability:stiffen(case, joint)
    if case.constraintHertz then
        joint.constraintHertz = case.constraintHertz
    end
end

-- The length of a rope or chain is the distance from its anchor to the center of its load, against the same distance at rest.
function JointStability:stretch(case)
    local dx, dy = case.load.x - case.x, case.load.y - case.top - 6
    local rest = case.kind == 'rope' and case.rest + 20 or case.rest
    return math.sqrt(dx * dx + dy * dy) / rest
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
    self:status(string.format('Lengths against rest: ropes %.2f and %.2f, chains %.2f and %.2f, load mass %.0f kg, step %.2f ms', self:stretch(cases[1]), self:stretch(cases[2]), self:stretch(cases[3]), self:stretch(cases[4]), cases[1].load.mass, self:stepTime()))
end

function JointStability:fixedUpdate(step)
    for _, case in ipairs(self.cases) do
        self:simulate(case.world, step)
    end
end

function JointStability:draw(area)
    graphics2d.drawLine(0, -430, 0, 430, 2, '#44FFFFFF')
    for _, case in ipairs(self.cases) do
        local good = case.x > 0
        graphics2d.drawText(nil, case.title, case.x, case.top - 70, {size = 24, color = good and '#FF6FDCA0' or '#FFFF8A84', anchor = {0.5, 0.5}})
        graphics2d.drawText(nil, string.format('%.2f times its length', self:stretch(case)), case.x, case.top - 38, {size = 20, color = '#CCFFFFFF', anchor = {0.5, 0.5}})
        parts.draw(case.anchor)
        if case.rope then
            local points = case.rope:points()
            for point = 2, #points do
                graphics2d.drawLine(points[point - 1].x, points[point - 1].y, points[point].x, points[point].y, 6, '#FFBCAAA4')
            end
        else
            parts.drawAll(case.links)
        end
        parts.draw(case.load, {layer = 1})
        case.grab:draw()
    end
end

return JointStability
