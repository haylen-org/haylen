-- Bodies sliding over the same rows of stone tiles in two worlds: on the left every tile is a box of its own and sliding boxes catch on the joints between them, and on the right each row is one chain loop around its tiles, as `map:buildCollision` merges the tiles of a Tiled map, so they slide to the end.
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local input = require('haylen.input')
local physics2d = require('haylen.physics2d')
local ui = require('haylen.ui')

local Look = require('categories.physics.look')
local PhysicsTest = require('categories.physics.physics-test')
local Scenery = require('categories.physics.scenery')

local TileSeams = haylen.class('TileSeams', PhysicsTest)

TileSeams.code = [[
floor:addBox(40, 40, {offsetX = x, offsetY = y})  -- One box per tile catches sliding boxes on the joints.
floor:addChain({{left, top}, {right, top}, {right, top + 40}, {left, top + 40}}, true)  -- One loop per row lets them slide.
local walls = map:buildCollision(world)  -- Merges the tiles of a Tiled map into loops like this.]]

TileSeams.tile = 40
TileSeams.tiles = 17
TileSeams.rows = {-200, -10, 180}
TileSeams.kinds = {'Tumbling boxes', 'Upright boxes', 'Balls'}
TileSeams.interval = 1.2
TileSeams.lifetime = 3
TileSeams.markLife = 2.5
TileSeams.ground = 350

function TileSeams:enter()
    self:frame{
        code = TileSeams.code,
        hint = 'Every 1.2 seconds each row launches a box that may tumble, a box that stays upright or a ball. Red marks show the joints where a body caught. R or the X button starts over.',
        controls = {
            ui.label{text = 'Launch speed', font = 'caption', color = 'textMuted'},
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
    self.halves = {}
    local tile = TileSeams.tile
    for index, merged in ipairs({false, true}) do
        local origin = index == 1 and -400 or 400
        local left, right = origin - TileSeams.tiles * tile / 2, origin + TileSeams.tiles * tile / 2
        local world = physics2d.newWorld()
        local floor = world:createBody({type = 'static'})
        for _, top in ipairs(TileSeams.rows) do
            if merged then
                floor:addChain({{left, top}, {right, top}, {right, top + tile}, {left, top + tile}}, true)
            else
                for column = 0, TileSeams.tiles - 1 do
                    floor:addBox(tile, tile, {offsetX = left + (column + 0.5) * tile, offsetY = top + tile / 2})
                end
            end
        end
        self.halves[index] = {world = world, origin = origin, left = left, right = right, shots = {}, marks = {}, distances = {0, 0, 0}, launched = {0, 0, 0}}
    end
    self.clock = 0
end

function TileSeams:launch()
    for _, half in ipairs(self.halves) do
        local x = half.left + 30
        for kind, top in ipairs(TileSeams.rows) do
            local body
            if kind == 3 then
                body = half.world:createBody({x = x, y = top - 16, vx = self.speed})
                body:addCircle(16, {friction = 0.2})
                Look.dress(body, 'yellow', {spin = true})
            else
                body = half.world:createBody({x = x, y = top - 18, vx = self.speed, fixedRotation = kind == 2})
                body:addBox(36, 36, {friction = 0})
                Look.dress(body, kind == 1 and 'red' or 'teal')
            end
            half.shots[#half.shots + 1] = {body = body, kind = kind, age = 0, start = x, top = top}
        end
    end
end

-- A shot ends at the end of its row or when its time runs out, adds how far it went, and leaves a mark where it caught short of the end.
function TileSeams:count(half, step)
    for index = #half.shots, 1, -1 do
        local shot = half.shots[index]
        shot.age = shot.age + step
        local body = shot.body
        if body.x > half.right or shot.age > TileSeams.lifetime then
            local reached = math.min(body.x, half.right)
            half.distances[shot.kind] = half.distances[shot.kind] + reached - shot.start
            half.launched[shot.kind] = half.launched[shot.kind] + 1
            if reached < half.right - TileSeams.tile then
                local joint = half.left + math.floor((body.x - half.left) / TileSeams.tile + 0.5) * TileSeams.tile
                half.marks[#half.marks + 1] = {x = joint, y = shot.top, life = TileSeams.markLife}
            end
            body:destroy()
            table.remove(half.shots, index)
        end
    end
    for index = #half.marks, 1, -1 do
        local mark = half.marks[index]
        mark.life = mark.life - step
        if mark.life <= 0 then
            table.remove(half.marks, index)
        end
    end
end

function TileSeams:average(half, kind)
    return half.launched[kind] > 0 and half.distances[kind] / half.launched[kind] or 0
end

-- The share of its row a kind of body gets along on average.
function TileSeams:share(half, kind)
    return math.min(1, self:average(half, kind) / (half.right - half.left - 30))
end

function TileSeams:update(dt)
    TileSeams.super.update(self, dt)
    if input.pressed('reset') then
        self:build()
    end
    local left, right = self.halves[1], self.halves[2]
    self:status(string.format('Tumbling boxes get %d%% of the way on the tiles and %d%% on the chains, upright boxes %d%% and %d%%.', math.floor(self:share(left, 1) * 100 + 0.5), math.floor(self:share(right, 1) * 100 + 0.5), math.floor(self:share(left, 2) * 100 + 0.5), math.floor(self:share(right, 2) * 100 + 0.5)))
end

function TileSeams:fixedUpdate(step)
    self.clock = self.clock + step
    if self.clock >= TileSeams.interval then
        self.clock = self.clock - TileSeams.interval
        self:launch()
    end
    for _, half in ipairs(self.halves) do
        self:simulate(half.world, step)
        self:count(half, step)
    end
end

-- Each row draws as its stone tiles, and under it a bar fills with the share of the row its bodies get along on average.
function TileSeams:drawRow(half, kind, top)
    local tile = TileSeams.tile
    for column = 0, TileSeams.tiles - 1 do
        Look.block(half.left + (column + 0.5) * tile, top + tile / 2, tile, tile, 'stone', 0)
    end
    local unit = Look.pixel
    local width = half.right - half.left
    local y = top + tile + 34
    graphics2d.drawShape({half.left, y - 7, width, 14}, {radius = 7, color = '#B32F3352', borderWidth = 2 * unit, borderColor = '#FF15172A', layer = 30})
    local share = self:share(half, kind)
    if share > 0 then
        graphics2d.drawShape({half.left + 2 * unit, y - 5, math.max(10, (width - 4 * unit) * share), 10}, {radius = 5, color = '#FFFFC93C', layer = 31})
    end
    self:label(string.format('%s: %d%% of the way', TileSeams.kinds[kind], math.floor(share * 100 + 0.5)), half.left, y + 26, {anchor = {0, 0.5}, size = 16})
end

function TileSeams:draw(area)
    Scenery.sky(self.camera, TileSeams.ground)
    Scenery.ground(TileSeams.ground)
    self:compare('One box for every tile', 'One chain around every row', 0, -330)
    local unit = Look.pixel
    for _, half in ipairs(self.halves) do
        for kind, top in ipairs(TileSeams.rows) do
            self:drawRow(half, kind, top)
        end
        for _, mark in ipairs(half.marks) do
            local alpha = math.min(1, mark.life)
            graphics2d.drawCircle(mark.x, mark.y, 8 * unit, {0.94, 0.28, 0.24, alpha}, {layer = 32})
            graphics2d.drawRing(mark.x, mark.y, 8 * unit, 2.5 * unit, {0.12, 0.11, 0.2, alpha}, {layer = 32})
        end
        for _, shot in ipairs(half.shots) do
            Look.draw(shot.body, {layer = 2})
        end
    end
end

return TileSeams
