-- Bodies sliding over the same rows of tiles in two worlds: on the left every tile is a box of its own and sliding boxes catch on the joints between them, and on the right each row is one chain loop around its tiles, as `map:buildCollision` merges the tiles of a Tiled map, so they slide to the end.
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local input = require('haylen.input')
local physics2d = require('haylen.physics2d')
local ui = require('haylen.ui')

local parts = require('categories.physics.parts')
local PhysicsTest = require('categories.physics.physics-test')

local TileSeams = haylen.class('TileSeams', PhysicsTest)

local kTile = 40
local kTiles = 18
local kRows = {-230, -20, 190}
local kKinds = {'Tumbling box', 'Upright box', 'Ball'}
local kInterval = 1.2
local kLifetime = 3

function TileSeams:enter()
    self:frame{
        hint = 'Every 1.2 seconds each row launches a body along its tiles: a box that may tumble, a box that stays upright and a ball. The joints between the tiles stop the boxes on the left, while the chains on the right carry them to the end. R or the X button starts over.',
        controls = {
            ui.label{text = 'Launch speed in units per second', font = 'caption', color = 'textMuted'},
            ui.slider{id = 'speed', value = 700, min = 200, max = 1600, step = 50, showValue = true, decimals = 0, onChange = function(event) self.speed = event.value end},
            ui.button{id = 'launch', text = 'Launch now', onClick = function() self:launch() end},
            ui.button{id = 'reset', text = 'Start over', onClick = function() self:build() end},
        },
        pointer = false,
        focus = 'speed',
    }
    self.speed = 700
    self:build()
end

-- Both sides have the same tiles, made of one box per tile on the left and of one chain loop around each row on the right.
function TileSeams:build()
    self.sides = {}
    for index, merged in ipairs({false, true}) do
        local origin = index == 1 and -400 or 400
        local left, right = origin - kTiles * kTile / 2, origin + kTiles * kTile / 2
        local world = physics2d.newWorld()
        local floor = world:createBody({type = 'static'})
        for _, top in ipairs(kRows) do
            for tile = 0, kTiles - 1 do
                local x = left + tile * kTile
                if merged then
                    parts.outline(floor, {{x, top}, {x + kTile, top}, {x + kTile, top + kTile}, {x, top + kTile}})
                else
                    parts.box(floor, kTile, kTile, {offsetX = x + kTile / 2, offsetY = top + kTile / 2})
                end
            end
            if merged then
                floor:addChain({{left, top}, {right, top}, {right, top + kTile}, {left, top + kTile}}, true)
            end
        end
        self.sides[index] = {world = world, floor = floor, origin = origin, left = left, right = right, shots = {}, distances = {0, 0, 0}, launched = {0, 0, 0}}
    end
    self.clock = 0
end

function TileSeams:launch()
    for _, side in ipairs(self.sides) do
        local x = side.left + 30
        for kind, top in ipairs(kRows) do
            local body
            if kind == 3 then
                body = side.world:createBody({x = x, y = top - 16, vx = self.speed})
                parts.circle(body, 16, {friction = 0.2})
                parts.paint(body, '#FFFFD54F')
            else
                body = side.world:createBody({x = x, y = top - 18, vx = self.speed, fixedRotation = kind == 2})
                parts.box(body, 36, 36, {friction = 0})
                parts.paint(body, kind == 1 and '#FFE57373' or '#FF4DD0E1')
            end
            side.shots[#side.shots + 1] = {body = body, kind = kind, age = 0, start = x}
        end
    end
end

-- A shot ends at the end of its row or when its time runs out, and adds how far it went.
function TileSeams:count(side, step)
    for index = #side.shots, 1, -1 do
        local shot = side.shots[index]
        shot.age = shot.age + step
        local body = shot.body
        if body.x > side.right or shot.age > kLifetime then
            side.distances[shot.kind] = side.distances[shot.kind] + math.min(body.x, side.right) - shot.start
            side.launched[shot.kind] = side.launched[shot.kind] + 1
            body:destroy()
            table.remove(side.shots, index)
        end
    end
end

function TileSeams:average(side, kind)
    return side.launched[kind] > 0 and side.distances[kind] / side.launched[kind] or 0
end

function TileSeams:update(dt)
    TileSeams.super.update(self, dt)
    if input.pressed('reset') then
        self:build()
    end
    local left, right = self.sides[1], self.sides[2]
    self:status(string.format('Average distance of the tumbling boxes %.0f on the tiles and %.0f on the chains, of the upright boxes %.0f and %.0f, step %.2f ms', self:average(left, 1), self:average(right, 1), self:average(left, 2), self:average(right, 2), self:stepTime()))
end

function TileSeams:fixedUpdate(step)
    self.clock = self.clock + step
    if self.clock >= kInterval then
        self.clock = self.clock - kInterval
        self:launch()
    end
    for _, side in ipairs(self.sides) do
        self:simulate(side.world, step)
        self:count(side, step)
    end
end

function TileSeams:draw(area)
    graphics2d.drawLine(0, -430, 0, 430, 2, '#44FFFFFF')
    local titles = {'One box per tile', 'One chain around each row'}
    for index, side in ipairs(self.sides) do
        graphics2d.drawText(nil, titles[index], side.origin, -390, {size = 28, color = index == 1 and '#FFFF8A84' or '#FF6FDCA0', anchor = {0.5, 0.5}})
        parts.draw(side.floor)
        for _, shot in ipairs(side.shots) do
            parts.draw(shot.body, {layer = 1})
        end
        for kind, top in ipairs(kRows) do
            local text = string.format('%s: %.0f units on average', kKinds[kind], self:average(side, kind))
            graphics2d.drawText(nil, text, side.left, top + kTile + 26, {size = 22, color = '#CCFFFFFF', anchor = {0, 0.5}})
        end
    end
end

return TileSeams
