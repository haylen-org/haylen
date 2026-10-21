-- A navigation mesh built on a worker thread from a boundary and obstacles, with funnel paths that keep an agent radius clear, and obstacles added or removed by the player.
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local input = require('haylen.input')
local m = require('haylen.math')
local navigation2d = require('haylen.navigation2d')
local profiler = require('haylen.debug')
local ui = require('haylen.ui')

local AlgorithmTest = require('categories.algorithms.algorithm-test')
local Art = require('categories.algorithms.art')

local NavMesh = haylen.class('NavMesh', AlgorithmTest)

NavMesh.code = [[
local mesh = navigation2d.buildNavMeshAsync(boundary, rocks):await()  -- Inside a coroutine.
local crate = mesh:addObstacle({{100, 0}, {180, 0}, {180, 60}, {100, 60}})
local x, y = mesh:closestPoint(400, 380)
local path, length = mesh:findPath(agent.x, agent.y, x, y, 20)  -- Keeps a radius of 20 clear.
mesh:removeObstacle(crate)]]

local kBoundary = {{-740, -400}, {740, -400}, {740, 400}, {140, 400}, {140, 300}, {-140, 300}, {-140, 400}, {-740, 400}}
local kSpeed = 260
-- The dashed ring of the agent radius, whose points the draw moves in place.
NavMesh.ringSteps = 48

function NavMesh:enter()
    self:frame{
        hint = 'Tap or click to send the agent there, or to add and remove crates with the other brushes. The rocks come from the first build on a worker thread. Paths turn only at corners and skip gaps narrower than the agent. R or the X button places new obstacles.',
        code = NavMesh.code,
        controls = {
            ui.formField{label = 'Brush', ui.radioGroup{id = 'brush', items = {{id = 'walk', text = 'Walk there'}, {id = 'add', text = 'Add a crate'}, {id = 'remove', text = 'Remove a crate'}}, selected = 'walk', onChange = function(event) self.brush = event.value end}},
            ui.formField{label = 'Agent radius', ui.slider{id = 'radius', value = 20, min = 0, max = 60, step = 2, showValue = true, decimals = 0, onChange = function(event)
                self.radius = event.value
                self:route()
            end}},
            ui.checkbox{id = 'triangles', text = 'Show the triangles', checked = true, onChange = function(event) self.showTriangles = event.checked end},
            ui.button{id = 'reset', text = 'New obstacles', onClick = function() self:build() end},
        },
        focus = 'brush',
    }
    self.brush, self.radius, self.showTriangles = 'walk', 20, true
    self.random = m.random(67)
    self.art = Art()
    self.agent = {x = -680, y = -340, facing = 1}
    self.ring = {}
    self:build()
end

function NavMesh:randomObstacle(x, y)
    local width, height, angle = self.random:range(60, 180), self.random:range(40, 120), self.random:range(0, math.pi)
    local cos, sin = math.cos(angle), math.sin(angle)
    local points = {}
    for _, corner in ipairs({{-1, -1}, {1, -1}, {1, 1}, {-1, 1}}) do
        local cx, cy = corner[1] * width / 2, corner[2] * height / 2
        points[#points + 1] = {x + cx * cos - cy * sin, y + cx * sin + cy * cos}
    end
    return points
end

-- The rocks build into the first mesh on a worker thread, and later crates rebuild it on the next query.
function NavMesh:build()
    self.rocks, self.crates = {}, {}
    -- Rocks keep apart from each other and from the corner where the agent starts, so none overlap.
    local centers = {}
    while #self.rocks < 14 do
        local x, y = self.random:range(-600, 600), self.random:range(-300, 250)
        local clear = math.abs(x + 680) + math.abs(y + 340) > 220
        for _, center in ipairs(centers) do
            clear = clear and (center[1] - x) ^ 2 + (center[2] - y) ^ 2 > 190 * 190
        end
        if clear then
            centers[#centers + 1] = {x, y}
            self.rocks[#self.rocks + 1] = self:randomObstacle(x, y)
        end
    end
    self.mesh, self.path = nil, nil
    local rocks, started = self.rocks, haylen.elapsed()
    self:spawn(function()
        local mesh = navigation2d.buildNavMeshAsync(kBoundary, rocks):await()
        if mesh == nil or rocks ~= self.rocks then
            return
        end
        self.mesh = mesh
        self.buildLatency = (haylen.elapsed() - started) * 1000
        -- The agent sets off for the far corner as soon as the mesh is ready, so the funnel shows at once.
        local x, y = mesh:closestPoint(660, 330)
        self.goal = {x, y}
        self:route()
    end)
end

function NavMesh:route()
    if self.mesh == nil or self.goal == nil then
        return
    end
    profiler.beginScope('navmesh')
    if self.mesh.dirty then
        self.mesh:build()
    end
    self.path, self.length = self.mesh:findPath(self.agent.x, self.agent.y, self.goal[1], self.goal[2], self.radius)
    profiler.endScope()
    self.waypoint = 2
end

function NavMesh:apply(x, y)
    if self.brush == 'walk' then
        local cx, cy = self.mesh:closestPoint(x, y)
        self.goal = {cx, cy}
    elseif self.brush == 'add' then
        local crate = {points = self:randomObstacle(x, y)}
        crate.id = self.mesh:addObstacle(crate.points)
        self.crates[#self.crates + 1] = crate
    else
        for index, crate in ipairs(self.crates) do
            if m.polygonContains(crate.points, {x, y}) then
                self.mesh:removeObstacle(crate.id)
                table.remove(self.crates, index)
                break
            end
        end
    end
    self:route()
end

function NavMesh:update(dt)
    NavMesh.super.update(self, dt)
    if input.pressed('reset') then
        self:build()
    end
    if self.mesh == nil then
        self:status('Building on a worker thread.')
        return
    end
    if self.pointer.pressed then
        self:apply(self.pointer.worldX, self.pointer.worldY)
    end
    self:walk(dt)
    self:status(string.format('%d triangles, %d obstacles, built first in %.0f ms, a query of %.3f ms, %s', self.mesh.triangleCount, self.mesh.obstacleCount, self.buildLatency or 0, self:timing('navmesh'), self.path and string.format('a path of %d corners, %.0f long', #self.path, self.length) or 'no path'))
end

function NavMesh:walk(dt)
    local target = self.path and self.path[self.waypoint]
    if target == nil then
        return
    end
    local dx, dy = target.x - self.agent.x, target.y - self.agent.y
    local distance = math.sqrt(dx * dx + dy * dy)
    local step = kSpeed * dt
    if distance <= step then
        self.agent.x, self.agent.y = target.x, target.y
        self.waypoint = self.waypoint + 1
    else
        self.agent.x, self.agent.y = self.agent.x + dx / distance * step, self.agent.y + dy / distance * step
    end
    if math.abs(dx) > 1 then
        self.agent.facing = dx < 0 and -1 or 1
    end
end

-- The walls around the boundary are dark rock, the floor inside it lighter, and the obstacles stand on it with a dark rim.
function NavMesh:draw(area)
    graphics2d.drawShape({-790, -450, 1580, 900}, {radius = 18, color = '#FF1A1F2C', layer = -2})
    graphics2d.drawPolygon(kBoundary, '#FF2E3A52', {layer = -1})
    for _, rock in ipairs(self.rocks) do
        graphics2d.drawPolygon(rock, '#FF7C86A3', {layer = 2})
        graphics2d.drawStroke(NavMesh.flat(rock), {width = 4, color = '#FF15172A', closed = true, join = 'round', layer = 2})
    end
    for _, crate in ipairs(self.crates) do
        graphics2d.drawPolygon(crate.points, '#FFB07A44', {layer = 2})
        graphics2d.drawStroke(NavMesh.flat(crate.points), {width = 4, color = '#FF5A3A20', closed = true, join = 'round', layer = 2})
    end
    if self.mesh and self.showTriangles then
        for _, triangle in ipairs(self.mesh:triangles()) do
            graphics2d.drawStroke(NavMesh.flat(triangle), {width = 2, color = '#38FFFFFF', closed = true, layer = 1})
        end
    end
    if self.path then
        graphics2d.drawStroke(NavMesh.flat(self.path), {width = 6, color = '#FFFFC93C', join = 'round', cap = 'round', layer = 3})
    end
    if self.goal then
        self.art:goal(self.goal[1], self.goal[2] + 6, 44, 4)
    end
    local agent = self.agent
    if self.radius > 0 then
        local ring = self.ring
        for step = 0, NavMesh.ringSteps - 1 do
            local angle = step / NavMesh.ringSteps * math.pi * 2
            ring[step * 2 + 1], ring[step * 2 + 2] = agent.x + math.cos(angle) * self.radius, agent.y + math.sin(angle) * self.radius
        end
        graphics2d.drawStroke(ring, {width = 3, color = '#FF2FB3C4', closed = true, dashes = {10, 8}, layer = 5})
    end
    Art.shadow(agent.x, agent.y, 36, 5)
    self.art:character(agent.x, agent.y + 4, 86, 'teal', 6, agent.facing < 0)
end

function NavMesh.flat(points)
    local flat = {}
    for _, point in ipairs(points) do
        flat[#flat + 1] = point.x or point[1]
        flat[#flat + 1] = point.y or point[2]
    end
    return flat
end

return NavMesh
