-- Bunnymark: bunnies that fall and bounce, from a few to many. The bulk mode does the math in plain Lua arrays and hands the positions to C++ with one float buffer copy, and the tables mode keeps one table per bunny, as the performance section of the Lua guide compares.
local collections = require('haylen.collections')
local debugging = require('haylen.debug')
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local input = require('haylen.input')
local ui = require('haylen.ui')

local sample = require('sample')

local Bunnymark = haylen.class('Bunnymark', sample.Test)

local kGravity = 900
local kModes = {{id = 'bulk', text = 'Float buffer'}, {id = 'tables', text = 'Tables'}}
local kCode = [[
buffer:set(1, positions)  -- one copy of a plain Lua array of x, y pairs
graphics2d.drawBatch(bunny, buffer, {fields = {'x', 'y'}, width = 32, height = 40})
graphics2d.drawBatch(bunny, sprites)  -- the tables mode: one {x, y, width, height} table per bunny]]

function Bunnymark:enter()
    self.texture = sample.texture('images/bunny.png')
    self.mode = 'bulk'
    self.count = 0
    self.positions, self.velocities, self.sprites = {}, {}, {}
    self.layout = {fields = {'x', 'y'}, width = 32, height = 40}
    self.buffer = collections.newFloatBuffer(0)
    self:frame({
        hint = 'Hold the mouse or a finger on the stage to pour bunnies, or use + and - or the shoulder buttons.',
        code = kCode,
        controls = {
            ui.segmentedControl{id = 'mode', items = kModes, selected = 'bulk', onChange = function(event) self.mode = event.value end},
            ui.row{gap = 12,
                ui.button{id = 'add', text = 'Add 1,000', grow = 1, variant = 'primary', onClick = function() self:add(1000) end},
                ui.button{id = 'many', text = 'Add 10,000', grow = 1, onClick = function() self:add(10000) end},
            },
            ui.row{gap = 12,
                ui.button{id = 'remove', text = 'Remove 1,000', grow = 1, onClick = function() self:remove(1000) end},
                ui.button{id = 'few', text = 'Just 10', grow = 1, onClick = function() self:remove(self.count - 10) end},
            },
        },
        focus = 'add',
    })
end

function Bunnymark:resize(area)
    if self.count == 0 then
        self:add(1000)
    end
end

-- Adds bunnies at a point, or at the top left of the stage.
function Bunnymark:add(amount, x, y)
    local positions, velocities, sprites = self.positions, self.velocities, self.sprites
    for index = self.count + 1, self.count + amount do
        positions[index * 2 - 1], positions[index * 2] = x or 40, y or 40
        velocities[index * 2 - 1], velocities[index * 2] = math.random() * 400 + 50, math.random() * 400 - 200
        sprites[index] = {x = x or 40, y = y or 40, width = 32, height = 40}
    end
    self:resizeBuffers(self.count + amount)
end

function Bunnymark:remove(amount)
    self:resizeBuffers(math.max(0, self.count - amount))
end

-- The float buffer holds exactly the bunnies to draw, so it is made again whenever the count changes.
function Bunnymark:resizeBuffers(count)
    for index = count + 1, self.count do
        self.positions[index * 2 - 1], self.positions[index * 2] = nil, nil
        self.velocities[index * 2 - 1], self.velocities[index * 2] = nil, nil
        self.sprites[index] = nil
    end
    self.count = count
    self.buffer = collections.newFloatBuffer(count * 2)
end

function Bunnymark:update(dt)
    Bunnymark.super.update(self, dt)
    if not self.area then
        return
    end
    local x, y, _, held = self:pointer()
    if held then
        self:add(100, x, y)
    end
    if input.pressed('add') then
        self:add(1000)
    end
    if input.pressed('remove') then
        self:remove(1000)
    end

    local positions, velocities = self.positions, self.velocities
    local width, height = self.area.width, self.area.height
    for index = 1, self.count * 2, 2 do
        local px, py = positions[index] + velocities[index] * dt, positions[index + 1] + velocities[index + 1] * dt
        local vy = velocities[index + 1] + kGravity * dt
        if px < 0 or px > width then
            velocities[index] = -velocities[index]
            px = px < 0 and 0 or width
        end
        if py > height then
            vy = -vy * 0.85
            py = height
        end
        positions[index], positions[index + 1], velocities[index + 1] = px, py, vy
    end

    if self.mode == 'bulk' then
        self.buffer:set(1, positions)
    else
        local sprites = self.sprites
        for index = 1, self.count do
            local sprite = sprites[index]
            sprite.x, sprite.y = positions[index * 2 - 1], positions[index * 2]
        end
    end
    local frame = debugging.frame()
    self:status(string.format('%d bunnies   %s   %.0f FPS   %.2f ms per frame', self.count, self.mode == 'bulk' and 'float buffer' or 'one table each', frame.fps, frame.milliseconds))
end

function Bunnymark:draw(area)
    if self.count == 0 then
        return
    end
    if self.mode == 'bulk' then
        graphics2d.drawBatch(self.texture, self.buffer, self.layout)
    else
        graphics2d.drawBatch(self.texture, self.sprites)
    end
end

return Bunnymark
