-- The three materials of a shape side by side: friction decides how far boxes slide, restitution how high balls bounce and density how a seesaw tips.
local haylen = require('haylen')
local graphics2d = require('haylen.graphics2d')
local input = require('haylen.input')
local physics2d = require('haylen.physics2d')
local ui = require('haylen.ui')

local Grab = require('grab')
local parts = require('parts')
local sample = require('sample')

local Materials = haylen.class('Materials', sample.Test)

local kFrictions = {0, 0.05, 0.1, 0.3, 0.6}
local kRestitutions = {0, 0.25, 0.5, 0.75, 0.95}
local kLabel = {size = 19, color = '#FFE8EAF2', anchor = {0.5, 0}, layer = 10}

function Materials:enter()
    Materials.super.enter(self, {
        hint = 'Watch each material play out, and drag the bodies to try again. R or X replays the scene.',
        controls = {
            ui.button{id = 'replay', text = 'Replay', onClick = function() self:build() end},
        },
        stats = true,
        focus = 'replay',
    })
    self:build()
end

function Materials:build()
    self.world = physics2d.newWorld()
    self.grab = Grab.new(self.world)
    self.bodies, self.labels = {}, {}

    local ground = self.world:createBody({type = 'static', x = 0, y = 410})
    parts.box(ground, 1600, 40)
    self.statics = {ground}
    self:buildFriction()
    self:buildRestitution()
    self:buildDensity()
end

-- Boxes start on a gentle ramp with the most slippery one lowest: the grippy ones stay, and the slippery ones slide down against the wall.
function Materials:buildFriction()
    local ramp = self.world:createBody({type = 'static'})
    parts.polygon(ramp, {{-780, -150}, {-300, 25}, {-300, 65}, {-780, -110}}, {friction = 1})
    local wall = self.world:createBody({type = 'static', x = -270, y = 160})
    parts.box(wall, 20, 460)
    self.statics[#self.statics + 1] = ramp
    self.statics[#self.statics + 1] = wall

    local angle = math.atan(175, 480)
    local cos, sin = math.cos(angle), math.sin(angle)
    for index, friction in ipairs(kFrictions) do
        local along = 60 + (#kFrictions - index) * 90
        local x, y = -780 + cos * along + sin * 21, -150 + sin * along - cos * 21
        local box = self.world:createBody({x = x, y = y, rotation = angle})
        parts.box(box, 40, 40, {friction = friction})
        self.bodies[#self.bodies + 1] = box
        self.labels[#self.labels + 1] = {body = box, text = string.format('%.2f', friction)}
    end
    self.titles = {{text = 'Friction', x = -560, y = -300}}
end

-- Balls fall from the same height and keep more of their speed as the restitution grows.
function Materials:buildRestitution()
    for index, restitution in ipairs(kRestitutions) do
        local ball = self.world:createBody({x = -170 + index * 70, y = -300})
        parts.circle(ball, 24, {restitution = restitution})
        self.bodies[#self.bodies + 1] = ball
        self.labels[#self.labels + 1] = {x = -170 + index * 70, y = 396, text = string.format('%.2f', restitution)}
    end
    self.titles[#self.titles + 1] = {text = 'Restitution', x = 40, y = -380}
end

-- A small dense box outweighs a large light one on the seesaw.
function Materials:buildDensity()
    local pivot = self.world:createBody({type = 'static', x = 540, y = 330})
    parts.polygon(pivot, {{0, -30}, {40, 60}, {-40, 60}})
    local plank = self.world:createBody({x = 540, y = 290})
    parts.box(plank, 440, 16)
    self.world:createJoint('revolute', pivot, plank, {ax = 540, ay = 290, enableLimit = true, lower = -0.4, upper = 0.4})

    local heavy = self.world:createBody({x = 380, y = 180})
    parts.box(heavy, 40, 40, {density = 12})
    local light = self.world:createBody({x = 680, y = 130})
    parts.box(light, 110, 110, {density = 0.5})
    self.statics[#self.statics + 1] = pivot
    self.bodies[#self.bodies + 1] = plank
    self.bodies[#self.bodies + 1] = heavy
    self.bodies[#self.bodies + 1] = light
    self.labels[#self.labels + 1] = {body = heavy, text = 'density 12'}
    self.labels[#self.labels + 1] = {body = light, text = 'density 0.5'}
    self.titles[#self.titles + 1] = {text = 'Density', x = 540, y = -300}
end

function Materials:exit()
    Materials.super.exit(self)
    self.world, self.grab, self.bodies, self.statics = nil, nil, nil, nil
end

function Materials:update(dt)
    Materials.super.update(self, dt)
    if input.pressed('reset') then
        self:build()
    end
    self.grab:update(self.pointer, self.camera)
    self:showStats(string.format('bodies %d\nstep %.2f ms', self.world.bodyCount, sample.milliseconds('physics step')))
end

function Materials:fixedUpdate(step)
    sample.step(self.world, step)
end

function Materials:render()
    self:beginWorld()
    parts.drawAll(self.statics)
    parts.drawAll(self.bodies)
    for _, label in ipairs(self.labels) do
        local x, y = label.x, label.y
        if label.body then
            x, y = label.body.x, label.body.y - 70
        end
        graphics2d.drawText(nil, label.text, x, y, kLabel)
    end
    for _, title in ipairs(self.titles) do
        graphics2d.drawText(nil, title.text, title.x, title.y, {size = 34, color = '#FFFFD54F', anchor = {0.5, 0}})
    end
    self.grab:draw()
end

return Materials
