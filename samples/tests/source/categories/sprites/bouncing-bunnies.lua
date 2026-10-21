-- Bouncing bunnies: bunnies that fall and bounce on a meadow, from a few to many thousands. The float buffer mode does the math in plain Lua arrays and hands the positions to C++ with one copy, and the tables mode keeps one table per bunny.
local collections = require('haylen.collections')
local debugging = require('haylen.debug')
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local input = require('haylen.input')
local localization = require('haylen.localization')
local m = require('haylen.math')
local ui = require('haylen.ui')

local Landscape = require('categories.sprites.landscape')
local SpriteTest = require('categories.sprites.sprite-test')

local BouncingBunnies = haylen.class('BouncingBunnies', SpriteTest)

BouncingBunnies.gravity = 900
BouncingBunnies.width = 32
BouncingBunnies.height = 40
-- The share of the bounces on the ground that kick a bunny back up, so the meadow keeps hopping.
BouncingBunnies.kick = 0.5
BouncingBunnies.modes = {{id = 'bulk', text = 'Float buffer'}, {id = 'tables', text = 'One table each'}}
BouncingBunnies.actions = {actions = {
    {name = 'add', type = 'button', bindings = {'key:equal', 'key:keypadAdd', 'button:rightShoulder'}},
    {name = 'remove', type = 'button', bindings = {'key:minus', 'key:keypadSubtract', 'button:leftShoulder'}},
}}

BouncingBunnies.code = [[
local layout = {fields = {'x', 'y'}, width = 32, height = 40, pivotY = 1}
local buffer = collections.newFloatBuffer(count * 2)
buffer:set(1, positions)  -- One copy of a plain Lua array of x, y pairs.
graphics2d.drawBatch(bunny, buffer, layout)
graphics2d.drawBatch(bunny, sprites)  -- One {x, y, width, height} table per bunny.]]

function BouncingBunnies:enter()
    self.texture = SpriteTest.texture('images/bunny.png')
    self.landscape = Landscape()
    self.random = m.random(8)
    self.mode = 'bulk'
    self.count = 0
    self.time = 0
    self.positions, self.velocities, self.sprites = {}, {}, {}
    self.layout = {fields = {'x', 'y'}, width = BouncingBunnies.width, height = BouncingBunnies.height, pivotY = 1}
    self.buffer = collections.newFloatBuffer(0)
    self:loadActions(BouncingBunnies.actions)
    self:frame{
        code = BouncingBunnies.code,
        hint = 'Hold the mouse or a finger on the meadow to pour bunnies, or use + and - or the shoulder buttons.',
        play = true,
        controls = {
            ui.formField{label = 'Positions', ui.segmentedControl{id = 'mode', items = BouncingBunnies.modes, selected = 'bulk', onChange = function(event) self.mode = event.value end}},
            ui.row{wrap = true, gap = 12,
                ui.button{id = 'add', text = 'Add 1,000', grow = 1, variant = 'primary', onClick = function() self:add(1000) end},
                ui.button{id = 'many', text = 'Add 10,000', grow = 1, onClick = function() self:add(10000) end},
            },
            ui.row{wrap = true, gap = 12,
                ui.button{id = 'remove', text = 'Remove 1,000', grow = 1, onClick = function() self:remove(1000) end},
                ui.button{id = 'few', text = 'Keep 10', grow = 1, onClick = function() self:keep(10) end},
            },
        },
    }
end

-- The ground the bunnies land on sits above the bottom of the stage, on the grass of the landscape.
function BouncingBunnies:resize(area)
    self.ground = area.height - math.max(40, area.height * 0.1)
    if self.count == 0 then
        self:add(1000)
    end
end

-- Adds bunnies at a point, or spread over the sky of the meadow.
function BouncingBunnies:add(amount, x, y)
    local positions, velocities, sprites = self.positions, self.velocities, self.sprites
    local random, half, height = self.random, BouncingBunnies.width / 2, BouncingBunnies.height
    for index = self.count + 1, self.count + amount do
        local px = x or random:range(half, self.area.width - half)
        local py = y or random:range(height, self.ground * 0.6)
        positions[index * 2 - 1], positions[index * 2] = px, py
        velocities[index * 2 - 1], velocities[index * 2] = random:range(-250, 250), random:range(-200, 100)
        sprites[index] = {x = px, y = py, width = BouncingBunnies.width, height = height, pivotY = 1}
    end
    self:resizeBuffers(self.count + amount)
end

function BouncingBunnies:remove(amount)
    self:resizeBuffers(math.max(0, self.count - amount))
end

-- Adds or removes bunnies until `count` of them bounce.
function BouncingBunnies:keep(count)
    if count > self.count then
        self:add(count - self.count)
    else
        self:resizeBuffers(count)
    end
end

-- The float buffer holds exactly the bunnies to draw, so it is made again whenever the count changes.
function BouncingBunnies:resizeBuffers(count)
    for index = count + 1, self.count do
        self.positions[index * 2 - 1], self.positions[index * 2] = nil, nil
        self.velocities[index * 2 - 1], self.velocities[index * 2] = nil, nil
        self.sprites[index] = nil
    end
    self.count = count
    self.buffer = collections.newFloatBuffer(count * 2)
end

-- Moves every bunny and bounces it on the edges of its sprite: the sides of the stage, the top and the ground under its feet.
function BouncingBunnies:move(dt)
    local positions, velocities, random = self.positions, self.velocities, self.random
    local left, right = BouncingBunnies.width / 2, self.area.width - BouncingBunnies.width / 2
    local top, ground = BouncingBunnies.height, self.ground
    local gravity, kick = BouncingBunnies.gravity * dt, BouncingBunnies.kick
    -- A bounce never throws a bunny higher than the sky above the meadow.
    local highest = -math.sqrt(2 * BouncingBunnies.gravity * (ground - top) * 0.9)
    for index = 1, self.count * 2, 2 do
        local px, py = positions[index] + velocities[index] * dt, positions[index + 1] + velocities[index + 1] * dt
        local vy = velocities[index + 1] + gravity
        if px < left or px > right then
            velocities[index] = -velocities[index]
            px = px < left and left or right
        end
        if py > ground then
            vy = -vy * 0.85
            py = ground
            if random:nextFloat() < kick then
                vy = math.max(highest, vy - random:range(0, 360))
            end
        elseif py < top then
            vy = math.abs(vy)
            py = top
        end
        positions[index], positions[index + 1], velocities[index + 1] = px, py, vy
    end
end

function BouncingBunnies:update(dt)
    BouncingBunnies.super.update(self, dt)
    if not self.area then
        return
    end
    self.time = self.time + dt
    local x, y, _, held = self:pointer()
    if held then
        self:add(100, x, math.min(y, self.ground))
    end
    if input.pressed('add') then
        self:add(1000)
    end
    if input.pressed('remove') then
        self:remove(1000)
    end

    self:move(dt)
    if self.mode == 'bulk' then
        self.buffer:set(1, self.positions)
    else
        local positions, sprites = self.positions, self.sprites
        for index = 1, self.count do
            local sprite = sprites[index]
            sprite.x, sprite.y = positions[index * 2 - 1], positions[index * 2]
        end
    end
    local frame = debugging.frame()
    local source = self.mode == 'bulk' and 'from one float buffer' or 'from one table each'
    self:status(string.format('%s bunnies drawn %s, at %.0f frames per second and %.2f ms per frame.', localization.formatNumber(self.count), source, frame.fps, frame.milliseconds))
end

function BouncingBunnies:draw(area)
    self.landscape:draw(area, self.ground, 0.5, self.time)
    if self.count == 0 then
        return
    end
    if self.mode == 'bulk' then
        graphics2d.drawBatch(self.texture, self.buffer, self.layout)
    else
        graphics2d.drawBatch(self.texture, self.sprites)
    end
end

return BouncingBunnies
