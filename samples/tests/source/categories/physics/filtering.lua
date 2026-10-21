-- Collision filtering: every color is a category, each floor only stops its own color, and groups make pairs that always or never collide.
local haylen = require('haylen')
local input = require('haylen.input')
local m = require('haylen.math')
local physics2d = require('haylen.physics2d')
local timer = require('haylen.timer')
local ui = require('haylen.ui')

local Grab = require('categories.physics.grab')
local Look = require('categories.physics.look')
local PhysicsTest = require('categories.physics.physics-test')
local Scenery = require('categories.physics.scenery')

local Filtering = haylen.class('Filtering', PhysicsTest)

Filtering.code = [[
floor:addBox(820, 20, {category = 2, mask = 2})  -- The red floor stops only red bodies.
ball:addCircle(18, {category = 2, mask = 1 | 2})  -- A red ball meets the ground and its floor.
shape.mask = 1 | 2 | 4 | 8  -- Now it hits the other balls too.
box:addBox(70, 70, {category = 16, mask = 1 | 16, group = -1})  -- A shared negative group never collides.
box:addBox(70, 70, {category = 16, mask = 1, group = 1})  -- A shared positive group always does.]]

Filtering.colors = {
    {name = 'red', category = 2, floor = -150},
    {name = 'green', category = 4, floor = 40},
    {name = 'blue', category = 8, floor = 230},
}
Filtering.groundCategory = 1
Filtering.maxBalls = 45
Filtering.ground = 360
Filtering.pairs = {{x = 470, group = -1, caption = 'Group -1\nnever collide'}, {x = 680, group = 1, caption = 'Group 1\nalways collide'}}
Filtering.ghost = Look.tint('purple', '#889A6BE0', '#996E44B0', '#AAD6BEFF')

function Filtering:enter()
    self:frame{
        code = Filtering.code,
        hint = 'Each floor masks every color but its own. The switches let a color hit the other balls too.',
        controls = {
            ui.toggle{id = 'red', text = 'Red hits other balls', onChange = function(event) self:setSocial(1, event.checked) end},
            ui.toggle{id = 'green', text = 'Green hits other balls', onChange = function(event) self:setSocial(2, event.checked) end},
            ui.toggle{id = 'blue', text = 'Blue hits other balls', onChange = function(event) self:setSocial(3, event.checked) end},
            ui.toggle{id = 'rain', text = 'Rain balls', checked = true, onChange = function(event) self.raining = event.checked end},
            ui.button{id = 'reset', text = 'Clear the balls', onClick = function() self:clear() end},
        },
        focus = 'red',
    }
    self.random = m.random(31)
    self.raining = true
    self.social = {false, false, false}
    self.world = physics2d.newWorld()
    self.grab = Grab(self.world)
    self.balls = {}
    self:build()
    timer.every(0.25, function()
        if self.raining then
            self:dropBall(self.random:integer(1, 3))
        end
    end, {owner = self})
end

function Filtering:build()
    self.statics = {}
    local ground = self.world:createBody({type = 'static', x = 0, y = Filtering.ground + 30})
    ground:addBox(8000, 60, {category = Filtering.groundCategory})
    for index, color in ipairs(Filtering.colors) do
        local floor = self.world:createBody({type = 'static', x = -200 + index * 30, y = color.floor, rotation = index % 2 == 0 and -0.12 or 0.12})
        floor:addBox(820, 20, {category = color.category, mask = color.category})
        Look.dress(floor, color.name)
        self.statics[#self.statics + 1] = floor
    end

    -- The pair of group -1 would collide by its masks but never does, and the pair of group 1 would not but always does. The top box of each pair starts half a box to the side, so the first passes through the lower box in plain view.
    self.groups = {}
    for _, pair in ipairs(Filtering.pairs) do
        local mask = pair.group < 0 and Filtering.groundCategory | 16 or Filtering.groundCategory
        for level = 0, 1 do
            local box = self.world:createBody({x = pair.x + level * (pair.group < 0 and 34 or 0), y = Filtering.ground - 36 - level * 140})
            box:addBox(70, 70, {category = 16, mask = mask, group = pair.group})
            Look.dress(box, pair.group < 0 and (level == 1 and Filtering.ghost or 'purple') or 'yellow')
            self.groups[#self.groups + 1] = box
        end
    end
end

function Filtering:dropBall(index)
    local color = Filtering.colors[index]
    local ball = self.world:createBody({x = self.random:range(-420, 300), y = -440})
    ball:addCircle(self.random:range(16, 22), {category = color.category, mask = self:mask(index), restitution = 0.2})
    Look.dress(ball, color.name)
    ball.data.team = index
    self.balls[#self.balls + 1] = ball
    if #self.balls > Filtering.maxBalls then
        table.remove(self.balls, 1):destroy()
    end
end

-- A ball always meets the ground and its own floor, and the other balls only while its switch is on.
function Filtering:mask(index)
    local colors = Filtering.colors
    local mask = Filtering.groundCategory | colors[index].category
    if self.social[index] then
        mask = mask | colors[1].category | colors[2].category | colors[3].category
    end
    return mask
end

function Filtering:setSocial(index, social)
    self.social[index] = social
    for _, ball in ipairs(self.balls) do
        if ball.data.team == index then
            for _, shape in ipairs(ball:shapes()) do
                shape.mask = self:mask(index)
            end
        end
    end
end

function Filtering:clear()
    for _, ball in ipairs(self.balls) do
        ball:destroy()
    end
    self.balls = {}
end

function Filtering:update(dt)
    Filtering.super.update(self, dt)
    if input.pressed('reset') then
        self:clear()
    end
    self.grab:update(self.pointer, self.camera)
    local counts = {0, 0, 0}
    for _, ball in ipairs(self.balls) do
        counts[ball.data.team] = counts[ball.data.team] + 1
    end
    self:status(string.format('%d red, %d green and %d blue balls fall onto the floors of their colors.', counts[1], counts[2], counts[3]))
end

function Filtering:fixedUpdate(step)
    self:simulate(self.world, step)
end

function Filtering:draw(area)
    Scenery.sky(self.camera, Filtering.ground)
    Scenery.ground(Filtering.ground)
    Look.drawAll(self.statics)
    Look.drawAll(self.balls, {layer = 1})
    Look.drawAll(self.groups, {layer = 2})
    for _, pair in ipairs(Filtering.pairs) do
        self:label(pair.caption, pair.x + (pair.group < 0 and 17 or 0), Filtering.ground - 250, {size = 17})
    end
    self.grab:draw()
end

return Filtering
