-- Ray casts without a physics world: against segments, rectangles, circles, polygons and chains of `math`, through the cells of a grid and the boxes of an AABB tree, with piercing, bounces and fans.
local haylen = require('haylen')
local graphics2d = require('haylen.graphics2d')
local input = require('haylen.input')
local m = require('haylen.math')
local profiler = require('haylen.debug')
local spatial2d = require('haylen.spatial2d')
local ui = require('haylen.ui')

local sample = require('sample')

local Raycasts = haylen.class('Raycasts', sample.Test)

local kOrigin = {-700, 0}
local kLength = 1600
local kRect = {-420, -300, 140, 90}
local kCircle = {center = {-180, 200}, radius = 80}
local kPolygon = {{-40, -220}, {60, -120}, {10, 0}, {-100, -60}}
local kChain = {{100, 380}, {200, 250}, {300, 330}, {400, 220}}
local kWalls = {{{-500, -420}, {-500, -120}}, {{-300, 100}, {-60, 360}}, {{150, -380}, {300, -150}}}
local kGrid = {columns = 12, rows = 16, cell = 32, x = 380, y = -256}
local kModes = {
    {id = 'closest', text = 'Closest of every shape'},
    {id = 'pierce', text = 'Pierce four segments'},
    {id = 'bounce', text = 'Bounce off everything'},
    {id = 'fan', text = 'Fan of rays'},
    {id = 'grid', text = 'Through grid cells'},
    {id = 'tree', text = 'Boxes of an AABB tree'},
}

-- Every shape as the segments of its outline, inside the walls of the stage, which the segment casts, bounces and fans read.
local function outline(points, closed, segments)
    for index = 1, #points - (closed and 0 or 1) do
        segments[#segments + 1] = {points[index], points[index % #points + 1]}
    end
end

local function allSegments()
    local segments = {}
    outline({{-780, -420}, {780, -420}, {780, 420}, {-780, 420}}, true, segments)
    for _, wall in ipairs(kWalls) do
        segments[#segments + 1] = wall
    end
    local x, y, w, h = kRect[1], kRect[2], kRect[3], kRect[4]
    outline({{x, y}, {x + w, y}, {x + w, y + h}, {x, y + h}}, true, segments)
    local circle = {}
    for index = 0, 23 do
        local angle = index / 24 * math.pi * 2
        circle[#circle + 1] = {kCircle.center[1] + math.cos(angle) * kCircle.radius, kCircle.center[2] + math.sin(angle) * kCircle.radius}
    end
    outline(circle, true, segments)
    outline(kPolygon, true, segments)
    outline(kChain, false, segments)
    return segments
end

function Raycasts:enter()
    Raycasts.super.enter(self, {
        hint = 'Aim with the pointer. Q and E or the shoulder buttons switch the cast. The closest cast asks every shape and keeps the nearest hit.',
        controls = {
            ui.radioGroup{id = 'mode', items = kModes, selected = 'closest', onChange = function(event) self.mode = event.value end},
        },
        stats = true,
        focus = 'mode',
    })
    self.mode = 'closest'
    self.segments = allSegments()
    self.random = m.random(109)
    self.cells = spatial2d.newCellGrid(kGrid.columns, kGrid.rows)
    for _ = 1, 50 do
        self.cells:set(self.random:integer(0, kGrid.columns - 1), self.random:integer(0, kGrid.rows - 1), 1)
    end
    self.tree = spatial2d.newAabbTree()
    for index = 1, 12 do
        local box = {-250 + (index % 4) * 110 + self.random:range(-20, 20), -400 + (index // 4) * 40 + self.random:range(-10, 10), self.random:range(30, 70), 24}
        self.tree:set({id = index, box = box}, box)
    end
    self.cast = {rays = {}}
end

function Raycasts:ends()
    local dx, dy = self.pointer.worldX - kOrigin[1], self.pointer.worldY - kOrigin[2]
    local length = math.max(math.sqrt(dx * dx + dy * dy), 1)
    return kOrigin, {kOrigin[1] + dx / length * kLength, kOrigin[2] + dy / length * kLength}, {dx / length, dy / length}
end

-- Each mode returns the rays to draw, as a start, an end and an optional hit, the hit points and a summary.
local casts = {}

function casts.closest(test)
    local from, to = test:ends()
    local best
    for _, hit in ipairs({m.raycastRect(from, to, kRect), m.raycastCircle(from, to, kCircle), m.raycastPolygon(from, to, kPolygon), m.raycastChain(from, to, kChain), m.raycastSegments(from, to, kWalls)}) do
        if best == nil or hit.distance < best.distance then
            best = hit
        end
    end
    return {rays = {{from, to, best}}, summary = best and string.format('hit at %.0f', best.distance) or 'no hit'}
end

function casts.pierce(test)
    local from, to = test:ends()
    local hits = m.raycastSegmentsAll(from, to, test.segments, 4)
    return {rays = {{from, to}}, points = hits, summary = string.format('pierced %d segments', #hits)}
end

function casts.bounce(test)
    local _, _, direction = test:ends()
    local hits, endX, endY = m.bounceRay(kOrigin, direction, 5000, 10, test.segments)
    local rays, start = {}, kOrigin
    for _, hit in ipairs(hits) do
        rays[#rays + 1] = {start, {hit.x, hit.y}, hit}
        start = {hit.x, hit.y}
    end
    rays[#rays + 1] = {start, {endX, endY}}
    return {rays = rays, summary = string.format('%d bounces', #hits)}
end

function casts.fan(test)
    local _, _, direction = test:ends()
    local angle = math.atan(direction[2], direction[1])
    local fan = m.rayFan(kOrigin, angle, math.rad(80), 41, 1200, test.segments)
    local rays, count = {}, 0
    for index, hit in ipairs(fan) do
        local rayAngle = angle + math.rad(80) * ((index - 1) / 40 - 0.5)
        rays[index] = {kOrigin, {kOrigin[1] + math.cos(rayAngle) * 1200, kOrigin[2] + math.sin(rayAngle) * 1200}, hit or nil}
        count = count + (hit and 1 or 0)
    end
    return {rays = rays, summary = string.format('%d of 41 rays hit', count)}
end

-- The grid cast walks the cells from the pointer toward the lower right, in the coordinates of the grid.
function casts.grid(test)
    local from = {test.pointer.worldX - kGrid.x, test.pointer.worldY - kGrid.y}
    local to = {from[1] + 900, from[2] + 300}
    local hit = spatial2d.raycastGrid(test.cells, from, to, kGrid.cell)
    local start, finish = {from[1] + kGrid.x, from[2] + kGrid.y}, {to[1] + kGrid.x, to[2] + kGrid.y}
    local drawn = hit and {x = hit.x + kGrid.x, y = hit.y + kGrid.y, normalX = hit.normalX, normalY = hit.normalY}
    return {rays = {{start, finish, drawn}}, summary = hit and string.format('cell %d, %d', hit.column, hit.row) or 'left the grid'}
end

function casts.tree(test)
    local from, to = test:ends()
    local hits = test.tree:raycast(from[1], from[2], to[1], to[2], 3)
    return {rays = {{from, to}}, points = hits, summary = string.format('%d boxes of %d', #hits, test.tree.size)}
end

function Raycasts:exit()
    Raycasts.super.exit(self)
    self.segments, self.cells, self.tree, self.cast = nil, nil, nil, nil
end

function Raycasts:update(dt)
    Raycasts.super.update(self, dt)
    if input.pressed('next') or input.pressed('previous') then
        local step = input.pressed('next') and 1 or -1
        for index, mode in ipairs(kModes) do
            if mode.id == self.mode then
                self.mode = kModes[(index - 1 + step) % #kModes + 1].id
                self:set('mode', {selected = self.mode})
                break
            end
        end
    end
    profiler.beginScope('math casts')
    self.cast = casts[self.mode](self)
    profiler.endScope()
    self:showStats(string.format('%s\ncast %.3f ms', self.cast.summary, self:timing('math casts')))
end

function Raycasts:render()
    self:beginWorld()
    local shape = '#FF5C6BC0'
    graphics2d.drawRectOutline({-780, -420, 1560, 840}, 3, shape)
    graphics2d.drawRect(kRect, shape)
    graphics2d.drawCircle(kCircle.center[1], kCircle.center[2], kCircle.radius, shape)
    graphics2d.drawPolygon(kPolygon, shape)
    graphics2d.drawPolyline(kChain, 5, shape)
    for _, wall in ipairs(kWalls) do
        graphics2d.drawLine(wall[1][1], wall[1][2], wall[2][1], wall[2][2], 5, shape)
    end
    for row = 0, kGrid.rows - 1 do
        for column = 0, kGrid.columns - 1 do
            local blocked = self.cells:get(column, row) == 1
            graphics2d.drawRect({kGrid.x + column * kGrid.cell + 1, kGrid.y + row * kGrid.cell + 1, kGrid.cell - 2, kGrid.cell - 2}, blocked and '#FF78909C' or '#FF242B36')
        end
    end
    for _, value in ipairs(self.tree:query({-2000, -2000, 4000, 4000})) do
        graphics2d.drawRectOutline(value.box, 2, '#FFFFB74D')
    end
    graphics2d.drawCircle(kOrigin[1], kOrigin[2], 12, '#FFFFFFFF', {layer = 5})
    for _, ray in ipairs(self.cast.rays) do
        graphics2d.drawLine(ray[1][1], ray[1][2], ray[2][1], ray[2][2], 1.5, '#66FF8A80', {layer = 4})
        local hit = ray[3]
        if hit then
            graphics2d.drawLine(ray[1][1], ray[1][2], hit.x, hit.y, 3, '#FFFF5252', {layer = 5})
            graphics2d.drawCircle(hit.x, hit.y, 6, '#FFFFFFFF', {layer = 6})
            graphics2d.drawLine(hit.x, hit.y, hit.x + hit.normalX * 30, hit.y + hit.normalY * 30, 2, '#FF69F0AE', {layer = 6})
        end
    end
    for index, hit in ipairs(self.cast.points or {}) do
        graphics2d.drawCircle(hit.x, hit.y, 8, '#FFFFD54F', {layer = 6})
        graphics2d.drawText(nil, tostring(index), hit.x + 10, hit.y - 28, {size = 22, layer = 6})
    end
end

return Raycasts
