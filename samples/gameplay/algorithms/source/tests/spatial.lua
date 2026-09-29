-- The four spatial structures of spatial2d holding hundreds of moving boxes and answering area, circle, point, ray and nearest queries under the pointer.
local haylen = require('haylen')
local graphics2d = require('haylen.graphics2d')
local input = require('haylen.input')
local m = require('haylen.math')
local profiler = require('haylen.debug')
local spatial2d = require('haylen.spatial2d')
local ui = require('haylen.ui')

local sample = require('sample')

local Spatial = haylen.class('Spatial', sample.Test)

local kCount = 400
local kArea = {-760, -410, 1520, 820}
local kHashCell = 80
local kStructures = {
    {id = 'hash', text = 'Spatial hash'},
    {id = 'quad', text = 'Quadtree'},
    {id = 'aabb', text = 'Dynamic AABB tree'},
    {id = 'kd', text = 'k-d tree'},
}
local kQueries = {
    {id = 'rect', text = 'Rectangle'},
    {id = 'circle', text = 'Circle'},
    {id = 'point', text = 'Point'},
    {id = 'ray', text = 'Ray, three hits'},
    {id = 'nearest', text = 'Five nearest'},
}

function Spatial:enter()
    Spatial.super.enter(self, {
        hint = 'The query follows the pointer. Q and E or the shoulder buttons switch the query, and the moving boxes update the structure every frame.',
        controls = {
            ui.combo{id = 'structure', items = kStructures, selected = 'hash', onChange = function(event) self:use(event.value) end},
            ui.radioGroup{id = 'query', items = kQueries, selected = 'rect', onChange = function(event) self.query = event.value end},
            ui.toggle{id = 'moving', text = 'Moving boxes', checked = true, onChange = function(event) self.moving = event.checked end},
        },
        stats = true,
        focus = 'structure',
    })
    self.query, self.moving = 'rect', true
    self.random = m.random(73)
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
    return string.format('built %s', tostring(structure.built))
end

function Spatial:exit()
    Spatial.super.exit(self)
    self.structure, self.entities, self.found = nil, nil, nil
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
        profiler.beginScope('spatial update')
        self:store()
        profiler.endScope()
    end
    self.hits = nil
    profiler.beginScope('spatial query')
    self.found = self:ask(self.pointer.worldX, self.pointer.worldY)
    profiler.endScope()

    local structure = self.structure
    self:showStats(string.format('stored %d\n%s\nfound %d\nupdate %.3f ms\nquery %.3f ms', structure.size, self:detail(), #self.found, self:timing('spatial update'), self:timing('spatial query')))
end

function Spatial:render()
    self:beginWorld()
    local found = {}
    for _, entity in ipairs(self.found) do
        found[entity] = true
    end
    if self.kind == 'hash' then
        for x = -800, 800, kHashCell do
            graphics2d.drawLine(x, -430, x, 430, 1, '#22FFFFFF')
        end
        for y = -400, 400, kHashCell do
            graphics2d.drawLine(-800, y, 800, y, 1, '#22FFFFFF')
        end
    end
    for _, entity in ipairs(self.entities) do
        local rect = {entity.x - entity.width / 2, entity.y - entity.height / 2, entity.width, entity.height}
        if found[entity] then
            graphics2d.drawRect(rect, '#FFFFB74D', {layer = 2})
        else
            graphics2d.drawRectOutline(rect, 2, '#FF78909C', {layer = 1})
        end
    end
    local x, y = self.pointer.worldX, self.pointer.worldY
    local order = {layer = 3}
    if self.query == 'rect' then
        graphics2d.drawRectOutline({x - 150, y - 100, 300, 200}, 3, '#FFFFFFFF', order)
    elseif self.query == 'circle' then
        graphics2d.drawRing(x, y, 150, 3, '#FFFFFFFF', order)
    elseif self.query == 'ray' then
        graphics2d.drawLine(-760, 0, x, y, 3, '#FFFFFFFF', order)
        for _, hit in ipairs(self.hits or {}) do
            graphics2d.drawCircle(hit.x, hit.y, 7, '#FFFF5252', order)
        end
    elseif self.query == 'nearest' then
        for _, entity in ipairs(self.found) do
            graphics2d.drawLine(x, y, entity.x, entity.y, 2, '#AAFFFFFF', order)
        end
    end
    graphics2d.drawCircle(x, y, 5, '#FFFFFFFF', order)
end

return Spatial
