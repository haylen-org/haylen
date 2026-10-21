-- The three materials of a shape side by side: friction decides how far blocks slide down a ramp, restitution how high balls bounce and density how a seesaw tips, each with a body of the viewer's own whose value a slider sets.
local haylen = require('haylen')
local input = require('haylen.input')
local physics2d = require('haylen.physics2d')
local ui = require('haylen.ui')

local Grab = require('categories.physics.grab')
local Look = require('categories.physics.look')
local PhysicsTest = require('categories.physics.physics-test')
local Scenery = require('categories.physics.scenery')

local Materials = haylen.class('Materials', PhysicsTest)

Materials.code = [[
block:addBox(52, 52, {friction = 0.05})  -- Slides down the ramp.
ball:addCircle(26, {restitution = 0.75})  -- Keeps three quarters of its speed in a bounce.
iron:addBox(48, 48, {density = 12})  -- Outweighs a large crate of density 0.5.
shape.friction = slider.value  -- Materials change while the body moves.
world:createJoint('revolute', pivot, plank, {ax = 560, ay = 228, enableLimit = true, lower = -0.22, upper = 0.22})]]

Materials.ground = 330
Materials.titleY = -330
-- Each friction with the material that has about it, from ice to rubber, and each restitution with the colors of a ball that bounces about that much, from clay to a super ball.
Materials.frictions = {{0, 'ice'}, {0.05, 'steel'}, {0.15, 'teal'}, {0.3, 'wood'}, {0.6, 'rubber'}}
Materials.restitutions = {{0, '#FFB06A3E', '#FF7E4626', '#FFE09A72'}, {0.25, '#FFE9A65B', '#FFB8743A', '#FFFFD9A3'}, {0.5, '#FFC8E04A', '#FF8FAA22', '#FFF0FFA0'}, {0.75, '#FFF0473E', '#FFB9302A', '#FFFF9A92'}, {0.95, '#FF9A6BE0', '#FF6E44B0', '#FFD6BEFF'}}
Materials.ramp = {{-780, -120}, {-300, 55}, {-300, 85}, {-780, -90}}
Materials.posts = {-740, -620, -500, -380}

function Materials:enter()
    self:frame{
        code = Materials.code,
        hint = 'Watch each material play out, set the values of your own block, ball and iron weight with the sliders, and drag the bodies to try again. R or the X button replays the scene.',
        controls = {
            ui.label{text = 'Friction of your block', font = 'caption', color = 'textMuted'},
            ui.slider{id = 'friction', value = 0.1, min = 0, max = 1, step = 0.05, showValue = true, decimals = 2, onChange = function(event) self:setFriction(event.value) end},
            ui.label{text = 'Bounce of your ball', font = 'caption', color = 'textMuted'},
            ui.slider{id = 'restitution', value = 0.6, min = 0, max = 1, step = 0.05, showValue = true, decimals = 2, onChange = function(event) self:setRestitution(event.value) end},
            ui.label{text = 'Density of the iron weight', font = 'caption', color = 'textMuted'},
            ui.slider{id = 'density', value = 12, min = 1, max = 20, step = 1, showValue = true, decimals = 0, onChange = function(event) self:setDensity(event.value) end},
            ui.button{id = 'replay', text = 'Replay', onClick = function() self:build() end},
        },
        focus = 'friction',
    }
    self.values = {friction = 0.1, restitution = 0.6, density = 12}
    self:build()
end

function Materials:build()
    self.world = physics2d.newWorld()
    self.grab = Grab(self.world)
    self.bodies, self.written = {}, {}

    local ground = self.world:createBody({type = 'static', x = 0, y = Materials.ground + 30})
    ground:addBox(8000, 60)
    self.statics = {}
    self:buildFriction()
    self:buildRestitution()
    self:buildDensity()
end

-- Blocks start on a gentle ramp with the most slippery one lowest: the grippy ones stay, and the slippery ones slide down against the wall.
function Materials:buildFriction()
    local ramp = self.world:createBody({type = 'static'})
    ramp:addPolygon(Materials.ramp, {friction = 1})
    Look.dress(ramp, 'wood')
    for _, x in ipairs(Materials.posts) do
        local top = -90 + (x + 780) * 175 / 480
        local post = self.world:createBody({type = 'static', x = x, y = (top + Materials.ground) / 2})
        post:addBox(22, Materials.ground - top)
        Look.dress(post, 'darkWood')
        self.statics[#self.statics + 1] = post
    end
    local wall = self.world:createBody({type = 'static', x = -270, y = (5 + Materials.ground) / 2})
    wall:addBox(30, Materials.ground - 5)
    Look.dress(wall, 'slate')
    self.statics[#self.statics + 1] = ramp
    self.statics[#self.statics + 1] = wall

    local angle = math.atan(175, 480)
    local cos, sin = math.cos(angle), math.sin(angle)
    local spots = {}
    for index = 1, #Materials.frictions + 1 do
        local along = 46 + (#Materials.frictions + 1 - index) * 72
        spots[index] = {-780 + cos * along + sin * 27, -120 + sin * along - cos * 27, along}
    end
    for index, spec in ipairs(Materials.frictions) do
        local spot = spots[index]
        local block = self.world:createBody({x = spot[1], y = spot[2], rotation = angle})
        block:addBox(52, 52, {friction = spec[1]})
        Look.dress(block, spec[2])
        self.bodies[#self.bodies + 1] = block
        self.written[#self.written + 1] = {body = block, text = string.format('%.2f', spec[1])}
    end
    local spot = spots[#spots]
    self.mine = self.world:createBody({x = spot[1], y = spot[2], rotation = angle})
    self.mineShape = self.mine:addBox(52, 52, {friction = self.values.friction})
    Look.dress(self.mine, 'yellow')
    self.bodies[#self.bodies + 1] = self.mine
    self.mineText = {body = self.mine, text = ''}
    self.written[#self.written + 1] = self.mineText
    self.titles = {{text = 'Friction', x = -540}}
end

-- Balls fall from the same height and keep more of their speed as the restitution grows.
function Materials:buildRestitution()
    for index, spec in ipairs(Materials.restitutions) do
        local x = -190 + index * 74
        local ball = self.world:createBody({x = x, y = -230})
        ball:addCircle(26, {restitution = spec[1]})
        Look.dress(ball, {fill = spec[2], shade = spec[3], light = spec[4]})
        self.bodies[#self.bodies + 1] = ball
        self.written[#self.written + 1] = {body = ball, text = string.format('%.2f', spec[1]), size = 15}
    end
    local x = -190 + (#Materials.restitutions + 1) * 74
    self.ball = self.world:createBody({x = x, y = -230})
    self.ballShape = self.ball:addCircle(26, {restitution = self.values.restitution})
    Look.dress(self.ball, 'yellow', {spin = true})
    self.bodies[#self.bodies + 1] = self.ball
    self.ballText = {body = self.ball, text = '', size = 15}
    self.written[#self.written + 1] = self.ballText
    self.titles[#self.titles + 1] = {text = 'Restitution', x = 70}
end

-- A small dense iron weight outweighs a large light crate on the seesaw.
function Materials:buildDensity()
    local top = Materials.ground - 70
    local pivot = self.world:createBody({type = 'static', x = 560, y = top})
    pivot:addPolygon({{0, -24}, {44, 70}, {-44, 70}})
    Look.dress(pivot, 'stone')
    local plank = self.world:createBody({x = 560, y = top - 32})
    plank:addBox(460, 18, {friction = 1})
    plank:addBox(12, 30, {offsetX = -224, offsetY = -24})
    plank:addBox(12, 30, {offsetX = 224, offsetY = -24})
    Look.dress(plank, 'wood')
    self.world:createJoint('revolute', pivot, plank, {ax = 560, ay = top - 32, enableLimit = true, lower = -0.22, upper = 0.22})

    self.iron = self.world:createBody({x = 375, y = top - 32 - 9 - 24 - 2})
    self.ironShape = self.iron:addBox(48, 48, {density = self.values.density, friction = 1})
    Look.dress(self.iron, 'iron')
    self.crate = self.world:createBody({x = 700, y = top - 32 - 9 - 58 - 2})
    self.crate:addBox(116, 116, {density = 0.5, friction = 1})
    Look.dress(self.crate, 'woodCrate')
    self.statics[#self.statics + 1] = pivot
    self.bodies[#self.bodies + 1] = plank
    self.bodies[#self.bodies + 1] = self.iron
    self.bodies[#self.bodies + 1] = self.crate
    self.ironText = {body = self.iron, text = ''}
    self.written[#self.written + 1] = self.ironText
    self.written[#self.written + 1] = {body = self.crate, text = '0.5', size = 22}
    self.titles[#self.titles + 1] = {text = 'Density', x = 560}
end

function Materials:setFriction(value)
    self.values.friction = value
    self.mineShape.friction = value
    self.mine.awake = true
end

function Materials:setRestitution(value)
    self.values.restitution = value
    self.ballShape.restitution = value
end

function Materials:setDensity(value)
    self.values.density = value
    self.ironShape.density = value
    self.iron.awake = true
end

function Materials:update(dt)
    Materials.super.update(self, dt)
    if input.pressed('reset') then
        self:build()
    end
    self.grab:update(self.pointer, self.camera)
    self.mineText.text = string.format('%.2f', self.values.friction)
    self.ballText.text = string.format('%.2f', self.values.restitution)
    self.ironText.text = string.format('%d', self.values.density)
    local tilt = self.iron.y < self.crate.y and 'the crate' or 'the iron weight'
    self:status(string.format('The seesaw leans toward %s, which holds %.0f kilograms against %.0f.', tilt, math.max(self.iron.mass, self.crate.mass), math.min(self.iron.mass, self.crate.mass)))
end

function Materials:fixedUpdate(step)
    self:simulate(self.world, step)
end

function Materials:draw(area)
    Scenery.sky(self.camera, Materials.ground)
    Scenery.ground(Materials.ground)
    Look.drawAll(self.statics)
    Look.drawAll(self.bodies, {layer = 1})
    for _, entry in ipairs(self.written) do
        if entry.body.valid then
            self:tag(entry.text, entry.body.x, entry.body.y, {size = entry.size or 16})
        end
    end
    for _, title in ipairs(self.titles) do
        self:label(title.text, title.x, Materials.titleY, {size = 26})
    end
    self.grab:draw()
end

return Materials
