-- The four spatial structures of `spatial2d` holding hundreds of moving boxes and answering area, circle, point, ray and nearest queries under the pointer.
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local input = require('haylen.input')
local m = require('haylen.math')
local profiler = require('haylen.debug')
local spatial2d = require('haylen.spatial2d')
local ui = require('haylen.ui')

local AlgorithmTest = require('categories.algorithms.algorithm-test')

local Spatial = haylen.class('Spatial', AlgorithmTest)

Spatial.code = [[
local hash = spatial2d.newHashGrid(80)  -- Or newQuadTree, newAabbTree and newKdTree.
hash:set(enemy, {enemy.x - 16, enemy.y - 16, 32, 32})  -- Setting it again moves it.
local inside = hash:query({x - 150, y - 100, 300, 200})
local around, nearest = hash:queryCircle(x, y, 150), hash:kNearest(x, y, 5)
local hits = hash:raycast(-760, 0, x, y, 3)  -- Each hit holds its value.]]

local kCount = 400
local kArea = {-760, -410, 1520, 820}
local kHashCell = 80
local kStructures = {
    {id = 'hash', text = 'Spatial hash'},
    {id = 'quad', text = 'Quadtree'},
    {id = 'aabb', text = 'Dynamic AABB tree'},
    {id = 'kd', text = 'K-d tree'},
}
local kQueries = {
    {id = 'rect', text = 'Rectangle'},
    {id = 'circle', text = 'Circle'},
    {id = 'point', text = 'Point'},
    {id = 'ray', text = 'Ray, three hits'},
    {id = 'nearest', text = 'Five nearest'},
}

function Spatial:enter()
    self:frame{
        hint = 'The query follows the pointer. Q and E or the shoulder buttons switch the query, and the moving boxes update the structure every frame.',
        code = Spatial.code,
        controls = {
            ui.combo{id = 'structure', items = kStructures, selected = 'hash', onChange = function(event) self:use(event.value) end},
            ui.radioGroup{id = 'query', items = kQueries, selected = 'rect', onChange = function(event) self.query = event.value end},
            ui.toggle{id = 'moving', text = 'Moving boxes', checked = true, onChange = function(event) self.moving = event.checked end},
        },
        focus = 'structure',
    }
    self.query, self.moving = 'rect', true
    self.random = m.random(73)
    self.queryX, self.queryY = 0, 0
    self.rect = {0, 0, 0, 0}
    self.plainBox = {radius = 4, color = '#FF2E3A52', borderWidth = 2, borderColor = '#FF6B7A99', layer = 1}
    self.litBox = {radius = 4, color = '#FFFFC93C', borderWidth = 2, borderColor = '#FF15172A', layer = 2}
    self.entities = {}
    for index = 1, kCount do
        local size = self.random:range(8, 50)
        self.entities[index] = {id = index, x = self.random:range(-700, 700), y = self.random:range(-360, 360), width = size, height = size * self.random:range(0.5, 1.5), vx = self.random:range(-60, 60), vy = self.random:range(-60, 60)}
    end
    self:use('hash')
end

function Spatial:use(kind)
    self.kind = kind
    if kind == 'hash' then
        self.structure = spatial2d.newHashGrid(kHashCell)
    elseif kind == 'quad' then
        self.structure = spatial2d.newQuadTree(kArea, {maxEntries = 6, maxDepth = 7})
    elseif kind == 'aabb' then
        self.structure = spatial2d.newAabbTree(6)
    else
        self.structure = spatial2d.newKdTree()
    end
    self:store()
end

-- Boxes live as rectangles in the first three structures and as circles around their centers in the k-d tree, which rebuilds after they move.
function Spatial:store()
    local structure = self.structure
    for _, entity in ipairs(self.entities) do
        if self.kind == 'kd' then
            structure:set(entity, entity.x, entity.y, math.max(entity.width, entity.height) / 2)
        else
            structure:set(entity, {entity.x - entity.width / 2, entity.y - entity.height / 2, entity.width, entity.height})
        end
    end
    if self.kind == 'kd' then
        structure:build()
    end
end

function Spatial:move(dt)
    for _, entity in ipairs(self.entities) do
        entity.x, entity.y = entity.x + entity.vx * dt, entity.y + entity.vy * dt
        if math.abs(entity.x) > 740 then
            entity.vx = -entity.vx
        end
        if math.abs(entity.y) > 390 then
            entity.vy = -entity.vy
        end
    end
end

function Spatial:ask(x, y)
    local structure = self.structure
    if self.query == 'rect' then
        return structure:query({x - 150, y - 100, 300, 200})
    elseif self.query == 'circle' then
        return structure:queryCircle(x, y, 150)
    elseif self.query == 'point' then
        return structure:queryPoint(x, y)
    elseif self.query == 'ray' then
        local hits, values = structure:raycast(-760, 0, x, y, 3), {}
        for index, hit in ipairs(hits) do
            values[index] = hit.value
        end
        self.hits = hits
        return values
    end
    return structure:kNearest(x, y, 5)
end

-- Each structure reports its own shape: the hash its cell size, the quadtree its quadrants and the AABB tree its height.
function Spatial:detail()
    local structure = self.structure
    if self.kind == 'hash' then
        return string.format('cell size %d', structure.cellSize)
    elseif self.kind == 'quad' then
        return string.format('quadrants %d', structure.nodeCount)
    elseif self.kind == 'aabb' then
        return string.format('tree height %d', structure.height)
    end
    return structure.built and 'built' or 'not built'
end

function Spatial:update(dt)
    Spatial.super.update(self, dt)
    if input.pressed('next') or input.pressed('previous') then
        local step = input.pressed('next') and 1 or -1
        for index, query in ipairs(kQueries) do
            if query.id == self.query then
                self.query = kQueries[(index - 1 + step) % #kQueries + 1].id
                self:set('query', {selected = self.query})
                break
            end
        end
    end
    if self.moving then
        self:move(dt)
        profiler.beginScope('spatialUpdate')
        self:store()
        profiler.endScope()
    end
    self.hits = nil
    profiler.beginScope('spatialQuery')
    -- The query sits in the middle until the pointer comes over the play area.
    self.queryX, self.queryY = self:aim(0, 0)
    self.found = self:ask(self.queryX, self.queryY)
    profiler.endScope()

    local structure = self.structure
    self:status(string.format('%d boxes stored, %s, %d found, updated in %.3f ms and asked in %.3f ms', structure.size, self:detail(), #self.found, self:timing('spatialUpdate'), self:timing('spatialQuery')))
end

function Spatial:draw(area)
    local found = {}
    for _, entity in ipairs(self.found) do
        found[entity] = true
    end
    graphics2d.drawShape({kArea[1] - 10, kArea[2] - 10, kArea[3] + 20, kArea[4] + 20}, {radius = 14, color = '#FF1B2130', borderWidth = 2, borderColor = '#FF3A4560', layer = -1})
    if self.kind == 'hash' then
        for x = kArea[1], kArea[1] + kArea[3], kHashCell do
            graphics2d.drawLine(x, kArea[2], x, kArea[2] + kArea[4], 1.5, '#40FFFFFF')
        end
        for y = kArea[2], kArea[2] + kArea[4], kHashCell do
            graphics2d.drawLine(kArea[1], y, kArea[1] + kArea[3], y, 1.5, '#40FFFFFF')
        end
    end
    local plain, lit = self.plainBox, self.litBox
    for _, entity in ipairs(self.entities) do
        local rect = self.rect
        rect[1], rect[2], rect[3], rect[4] = entity.x - entity.width / 2, entity.y - entity.height / 2, entity.width, entity.height
        graphics2d.drawShape(rect, found[entity] and lit or plain)
    end
    local x, y = self.queryX, self.queryY
    local order = {layer = 3}
    local edge = {width = 3, color = '#FFFFFFFF', closed = true, dashes = {12, 8}, layer = 3}
    if self.query == 'rect' then
        graphics2d.drawStroke({x - 150, y - 100, x + 150, y - 100, x + 150, y + 100, x - 150, y + 100}, edge)
    elseif self.query == 'circle' then
        local ring = {}
        for step = 0, 47 do
            ring[#ring + 1] = x + math.cos(step / 48 * math.pi * 2) * 150
            ring[#ring + 1] = y + math.sin(step / 48 * math.pi * 2) * 150
        end
        graphics2d.drawStroke(ring, edge)
    elseif self.query == 'ray' then
        graphics2d.drawStroke({-760, 0, x, y}, {width = 3, color = '#FFFFFFFF', cap = 'round', layer = 3})
        for _, hit in ipairs(self.hits or {}) do
            graphics2d.drawShape({hit.x - 8, hit.y - 8, 16, 16}, {kind = 'ellipse', color = '#FFFF6B5E', borderWidth = 2, borderColor = '#FF15172A', layer = 4})
        end
    elseif self.query == 'nearest' then
        for _, entity in ipairs(self.found) do
            graphics2d.drawStroke({x, y, entity.x, entity.y}, {width = 2, color = '#CCFFFFFF', cap = 'round', layer = 3})
        end
    end
    graphics2d.drawShape({x - 7, y - 7, 14, 14}, {kind = 'ellipse', color = '#FFFFFFFF', borderWidth = 2, borderColor = '#FF15172A', layer = 4})
end

return Spatial
