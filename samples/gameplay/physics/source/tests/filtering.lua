-- Collision filtering: every color is a category, each floor only stops its own color, and groups make pairs that always or never collide.
local haylen = require('haylen')
local graphics2d = require('haylen.graphics2d')
local input = require('haylen.input')
local m = require('haylen.math')
local physics2d = require('haylen.physics2d')
local timer = require('haylen.timer')
local ui = require('haylen.ui')

local Grab = require('grab')
local parts = require('parts')
local sample = require('sample')

local Filtering = haylen.class('Filtering', sample.Test)

local kColors = {
    {name = 'red', category = 2, color = '#FFE57373', floor = -120},
    {name = 'green', category = 4, color = '#FF81C784', floor = 80},
    {name = 'blue', category = 8, color = '#FF64B5F6', floor = 280},
}
local kFloorCategory = 1
local kMaxBalls = 45

function Filtering:enter()
    Filtering.super.enter(self, {
        hint = 'Red bodies land on the red floor, green on the green one and blue on the blue one, since each floor masks every other color. The switches let a color collide with the other balls too.',
        controls = {
            ui.toggle{id = 'red', text = 'Red hits other balls', onChange = function(event) self:setSocial(1, event.checked) end},
            ui.toggle{id = 'green', text = 'Green hits other balls', onChange = function(event) self:setSocial(2, event.checked) end},
            ui.toggle{id = 'blue', text = 'Blue hits other balls', onChange = function(event) self:setSocial(3, event.checked) end},
            ui.toggle{id = 'rain', text = 'Rain balls', checked = true, onChange = function(event) self.raining = event.checked end},
            ui.button{id = 'reset', text = 'Clear the balls', onClick = function() self:clear() end},
        },
        stats = true,
        focus = 'red',
    })
    self.random = m.random(31)
    self.raining = true
    self.social = {false, false, false}
    self.world = physics2d.newWorld()
    self.grab = Grab.new(self.world)
    self.balls = {}
    self:build()
    timer.every(0.25, function()
        if self.raining then
            self:spawn(self.random:integer(1, 3))
        end
    end, {owner = self})
end

function Filtering:build()
    self.statics = {}
    local ground = self.world:createBody({type = 'static', x = 0, y = 410})
    parts.box(ground, 1600, 40, {category = kFloorCategory})
    self.statics[1] = ground
    for index, color in ipairs(kColors) do
        local floor = self.world:createBody({type = 'static', x = -120 + index * 60, y = color.floor, rotation = index % 2 == 0 and -0.12 or 0.12})
        parts.box(floor, 900, 18, {category = color.category, mask = color.category})
        parts.paint(floor, color.color)
        self.statics[#self.statics + 1] = floor
    end

    -- The purple pair would collide by its masks but shares a negative group, and the yellow pair would not but shares a positive group.
    self.groups = {}
    for index, group in ipairs({-1, 1}) do
        local mask = group < 0 and kFloorCategory | 16 or kFloorCategory
        for pair = 0, 1 do
            local box = self.world:createBody({x = 540 + (index - 1) * 140, y = -300 - pair * 120})
            parts.box(box, 70, 70, {category = 16, mask = mask, group = group})
            parts.paint(box, group < 0 and '#FFBA68C8' or '#FFFFD54F')
            self.groups[#self.groups + 1] = box
        end
    end
end

function Filtering:spawn(index)
    local color = kColors[index]
    local ball = self.world:createBody({x = self.random:range(-350, 350), y = -420})
    parts.circle(ball, self.random:range(14, 22), {category = color.category, mask = self:mask(index), restitution = 0.2})
    parts.paint(ball, color.color)
    ball.data.team = index
    self.balls[#self.balls + 1] = ball
    if #self.balls > kMaxBalls then
        table.remove(self.balls, 1):destroy()
    end
end

-- A ball always meets the ground and its own floor, and the other balls only while its switch is on.
function Filtering:mask(index)
    local mask = kFloorCategory | kColors[index].category
    if self.social[index] then
        mask = mask | kColors[1].category | kColors[2].category | kColors[3].category
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

function Filtering:exit()
    Filtering.super.exit(self)
    self.world, self.grab, self.balls, self.statics, self.groups = nil, nil, nil, nil, nil
end

function Filtering:update(dt)
    Filtering.super.update(self, dt)
    if input.pressed('reset') then
        self:clear()
    end
    self.grab:update(self.pointer, self.camera)
    self:showStats(string.format('Balls %d\nStep %.2f ms', #self.balls, sample.milliseconds('physics step')))
end

function Filtering:fixedUpdate(step)
    sample.step(self.world, step)
end

function Filtering:render()
    self:beginWorld()
    parts.drawAll(self.statics)
    parts.drawAll(self.balls, {layer = 1})
    parts.drawAll(self.groups, {layer = 1})
    graphics2d.drawText(nil, 'Group -1\nNever collide', 540, 170, {size = 20, anchor = {0.5, 0}, align = 'center'})
    graphics2d.drawText(nil, 'Group 1\nAlways collide', 680, 170, {size = 20, anchor = {0.5, 0}, align = 'center'})
    self.grab:draw()
end

return Filtering
