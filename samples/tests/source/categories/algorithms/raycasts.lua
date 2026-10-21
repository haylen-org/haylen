-- Ray casts without a physics world: against segments, rectangles, circles, polygons and chains of `math`, through the cells of a grid and the boxes of an AABB tree, with piercing, bounces and fans.
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local input = require('haylen.input')
local m = require('haylen.math')
local profiler = require('haylen.debug')
local spatial2d = require('haylen.spatial2d')
local ui = require('haylen.ui')

local AlgorithmTest = require('categories.algorithms.algorithm-test')
local Art = require('categories.algorithms.art')

local Raycasts = haylen.class('Raycasts', AlgorithmTest)

Raycasts.code = [[
local hit = m.raycastCircle({-700, 0}, {900, 0}, {center = {-180, 200}, radius = 80})  -- A hit table or nil.
local pierced = m.raycastSegmentsAll(from, to, walls, 4)  -- The first four segments it crosses.
local bounces, endX, endY = m.bounceRay(from, {1, -0.4}, 5000, 10, walls)
local fan = m.rayFan(from, angle, math.rad(80), 41, 1200, walls)  -- A hit or false per ray.
local cell = spatial2d.raycastGrid(cells, from, to, 32)  -- The hit also has its column and row.
local boxes = tree:raycast(from[1], from[2], to[1], to[2], 3)]]

local kOrigin = {-700, 0}
local kLength = 1600
local kRect = {-420, -300, 140, 90}
local kCircle = {center = {-180, 200}, radius = 80}
local kPolygon = {{-40, -220}, {60, -120}, {10, 0}, {-100, -60}}
local kChain = {{100, 380}, {200, 250}, {300, 330}, {400, 220}}
local kWalls = {{{-500, -400}, {-500, -120}}, {{-300, 100}, {-60, 360}}, {{150, -380}, {300, -150}}}
local kFrame = {-780, -420, 1560, 840}
-- Where the ray aims while the pointer is away from the play area: at the circle, past the rectangle.
local kAim = {-180, 200}
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
    local x, y, w, h = kFrame[1], kFrame[2], kFrame[3], kFrame[4]
    outline({{x, y}, {x + w, y}, {x + w, y + h}, {x, y + h}}, true, segments)
    for _, wall in ipairs(kWalls) do
        segments[#segments + 1] = wall
    end
    x, y, w, h = kRect[1], kRect[2], kRect[3], kRect[4]
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
    self:frame{
        hint = 'Aim with the pointer. Q and E or the shoulder buttons switch the cast. The closest cast asks every shape and keeps the nearest hit.',
        code = Raycasts.code,
        controls = {
            ui.radioGroup{id = 'mode', items = kModes, selected = 'closest', onChange = function(event) self.mode = event.value end},
        },
        focus = 'mode',
    }
    self.mode = 'closest'
    self.segments = allSegments()
    self.frameSegments = {}
    local x, y, w, h = kFrame[1], kFrame[2], kFrame[3], kFrame[4]
    outline({{x, y}, {x + w, y}, {x + w, y + h}, {x, y + h}}, true, self.frameSegments)
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
    self.art = Art()
    self.polygonStroke, self.chainStroke = Raycasts.flat(kPolygon), Raycasts.flat(kChain)
end

function Raycasts.flat(points)
    local flat = {}
    for _, point in ipairs(points) do
        flat[#flat + 1] = point[1]
        flat[#flat + 1] = point[2]
    end
    return flat
end

function Raycasts:ends()
    local x, y = self:aim(kAim[1], kAim[2])
    local dx, dy = x - kOrigin[1], y - kOrigin[2]
    local length = math.max(math.sqrt(dx * dx + dy * dy), 1)
    local to = {kOrigin[1] + dx / length * kLength, kOrigin[2] + dy / length * kLength}
    local edge = m.raycastSegments(kOrigin, to, self.frameSegments)
    return kOrigin, edge and {edge.x, edge.y} or to, {dx / length, dy / length}
end

-- Each mode returns the rays to draw, as a start, an end and an optional hit, the hit points and a summary.
local casts = {}

function casts.closest(test)
    local from, to = test:ends()
    local best, name
    local hits = {rectangle = m.raycastRect(from, to, kRect), circle = m.raycastCircle(from, to, kCircle), polygon = m.raycastPolygon(from, to, kPolygon), chain = m.raycastChain(from, to, kChain), wall = m.raycastSegments(from, to, kWalls)}
    for shape, hit in pairs(hits) do
        if best == nil or hit.distance < best.distance then
            best, name = hit, shape
        end
    end
    return {rays = {{from, to, best}}, summary = best and string.format('The ray hits the %s %.0f units away', name, best.distance) or 'The ray reaches the frame without a hit'}
end

function casts.pierce(test)
    local from, to = test:ends()
    local hits = m.raycastSegmentsAll(from, to, test.segments, 4)
    return {rays = {{from, to}}, points = hits, summary = string.format('The ray pierces %d segments', #hits)}
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
    return {rays = rays, summary = string.format('The ray bounces %d times', #hits)}
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
    return {rays = rays, summary = string.format('%d of the 41 rays hit something', count)}
end

-- The grid cast walks the cells from the pointer toward the lower right, in the coordinates of the grid.
function casts.grid(test)
    local x, y = test:aim(kGrid.x - 120, kGrid.y + 40)
    local from = {x - kGrid.x, y - kGrid.y}
    local to = {from[1] + 900, from[2] + 300}
    local hit = spatial2d.raycastGrid(test.cells, from, to, kGrid.cell)
    local start, finish = {from[1] + kGrid.x, from[2] + kGrid.y}, {to[1] + kGrid.x, to[2] + kGrid.y}
    local drawn = hit and {x = hit.x + kGrid.x, y = hit.y + kGrid.y, normalX = hit.normalX, normalY = hit.normalY}
    return {rays = {{start, finish, drawn}}, summary = hit and string.format('The ray stops in the cell of column %d and row %d', hit.column, hit.row) or 'The ray leaves the grid without a hit'}
end

function casts.tree(test)
    local from, to = test:ends()
    local hits = test.tree:raycast(from[1], from[2], to[1], to[2], 3)
    return {rays = {{from, to}}, points = hits, summary = string.format('The ray crosses %d of the %d boxes', #hits, test.tree.size)}
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
    profiler.beginScope('mathCasts')
    self.cast = casts[self.mode](self)
    profiler.endScope()
    self:status(string.format('%s, cast in %.3f ms', self.cast.summary, self:timing('mathCasts')))
end

function Raycasts:draw(area)
    local fill, edge = '#FF2E3A58', '#FF8FA4E8'
    graphics2d.drawShape(kFrame, {radius = 6, color = '#FF181D28', borderWidth = 4, borderColor = '#FF4A5578', layer = -1})
    graphics2d.drawShape(kRect, {radius = 6, color = fill, borderWidth = 4, borderColor = edge})
    graphics2d.drawShape({kCircle.center[1] - kCircle.radius, kCircle.center[2] - kCircle.radius, kCircle.radius * 2, kCircle.radius * 2}, {kind = 'ellipse', color = fill, borderWidth = 4, borderColor = edge})
    graphics2d.drawPolygon(kPolygon, fill)
    graphics2d.drawStroke(self.polygonStroke, {width = 4, color = edge, closed = true, join = 'round', layer = 1})
    graphics2d.drawStroke(self.chainStroke, {width = 6, color = edge, join = 'round', cap = 'round'})
    for _, wall in ipairs(kWalls) do
        graphics2d.drawStroke({wall[1][1], wall[1][2], wall[2][1], wall[2][2]}, {width = 6, color = edge, cap = 'round'})
    end
    for row = 0, kGrid.rows - 1 do
        for column = 0, kGrid.columns - 1 do
            local blocked = self.cells:get(column, row) == 1
            graphics2d.drawShape({kGrid.x + column * kGrid.cell + 2, kGrid.y + row * kGrid.cell + 2, kGrid.cell - 4, kGrid.cell - 4}, {radius = 4, color = blocked and '#FF8FA4E8' or '#FF232B3D'})
        end
    end
    for _, value in ipairs(self.tree:query({-2000, -2000, 4000, 4000})) do
        graphics2d.drawShape(value.box, {radius = 4, color = '#33FFC93C', borderWidth = 2, borderColor = '#FFFFC93C'})
    end

    -- The ray runs faint to its end and bright to its hit, where the normal points away from the surface.
    for _, ray in ipairs(self.cast.rays) do
        graphics2d.drawStroke({ray[1][1], ray[1][2], ray[2][1], ray[2][2]}, {width = 3, color = '#66FF8A80', cap = 'round', layer = 4})
        local hit = ray[3]
        if hit then
            graphics2d.drawStroke({ray[1][1], ray[1][2], hit.x, hit.y}, {width = 5, color = '#FFFF6B5E', cap = 'round', layer = 5})
            graphics2d.drawStroke({hit.x, hit.y, hit.x + hit.normalX * 44, hit.y + hit.normalY * 44}, {width = 4, color = '#FF6FDCA0', cap = 'round', layer = 6})
            graphics2d.drawShape({hit.x - 10, hit.y - 10, 20, 20}, {kind = 'ellipse', color = '#FFFFFFFF', borderWidth = 3, borderColor = '#FFFF6B5E', layer = 7})
        end
    end
    for index, hit in ipairs(self.cast.points or {}) do
        graphics2d.drawShape({hit.x - 10, hit.y - 10, 20, 20}, {kind = 'ellipse', color = '#FFFFC93C', borderWidth = 3, borderColor = '#FF15172A', layer = 7})
        self.art:label(tostring(index), hit.x + 22, hit.y - 22, 22, '#FFFFFFFF', 8)
    end
    graphics2d.drawShape({kOrigin[1] - 16, kOrigin[2] - 16, 32, 32}, {kind = 'ellipse', color = '#FFF2F3FF', borderWidth = 4, borderColor = '#FF15172A', layer = 8})
end

return Raycasts
