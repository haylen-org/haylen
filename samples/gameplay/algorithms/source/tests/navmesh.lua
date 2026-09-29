-- A navigation mesh built on a worker thread from a boundary and obstacles, with funnel paths that keep an agent radius clear, and obstacles added or removed by the player.
local haylen = require('haylen')
local async = require('async')
local graphics2d = require('haylen.graphics2d')
local input = require('haylen.input')
local m = require('haylen.math')
local navigation2d = require('haylen.navigation2d')
local profiler = require('haylen.debug')
local ui = require('haylen.ui')

local sample = require('sample')

local NavMesh = haylen.class('NavMesh', sample.Test)

local kBoundary = {{-740, -400}, {740, -400}, {740, 400}, {140, 400}, {140, 300}, {-140, 300}, {-140, 400}, {-740, 400}}
local kSpeed = 260

function NavMesh:enter()
    NavMesh.super.enter(self, {
        hint = 'Tap or click to send the agent there, or to add and remove crates with the other brushes. The rocks come from the first build on a worker thread. Paths turn only at corners and skip gaps narrower than the agent.',
        controls = {
            ui.radioGroup{id = 'brush', items = {{id = 'walk', text = 'Walk there'}, {id = 'add', text = 'Add a crate'}, {id = 'remove', text = 'Remove a crate'}}, selected = 'walk', onChange = function(event) self.brush = event.value end},
            ui.label{text = 'Agent radius', font = 'caption', color = 'textMuted'},
            ui.slider{id = 'radius', value = 20, min = 0, max = 60, step = 2, showValue = true, decimals = 0, onChange = function(event)
                self.radius = event.value
                self:route()
            end},
            ui.checkbox{id = 'triangles', text = 'Show the triangles', checked = true, onChange = function(event) self.showTriangles = event.checked end},
            ui.button{id = 'reset', text = 'New obstacles', onClick = function() self:build() end},
        },
        stats = true,
        focus = 'brush',
    })
    self.brush, self.radius, self.showTriangles = 'walk', 20, true
    self.random = m.random(67)
    self.agent = {x = -680, y = -340}
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
    for index = 1, 14 do
        self.rocks[index] = self:randomObstacle(self.random:range(-600, 600), self.random:range(-300, 250))
    end
    self.mesh, self.path = nil, nil
    local started = haylen.time()
    async.spawn(function()
        local mesh = navigation2d.buildNavMeshAsync(kBoundary, self.rocks):await()
        if mesh == nil or self.rocks == nil then
            return
        end
        self.mesh = mesh
        self.buildLatency = (haylen.time() - started) * 1000
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

function NavMesh:exit()
    NavMesh.super.exit(self)
    self.mesh, self.rocks, self.crates, self.path = nil, nil, nil, nil
end

function NavMesh:update(dt)
    NavMesh.super.update(self, dt)
    if input.pressed('reset') then
        self:build()
    end
    if self.mesh == nil then
        self:showStats('building on a worker thread')
        return
    end
    if self.pointer.pressed then
        self:apply(self.pointer.worldX, self.pointer.worldY)
    end
    self:walk(dt)
    self:showStats(string.format('triangles %d\nobstacles %d\nfirst build %.0f ms\nquery %.3f ms\npath %s', self.mesh.triangleCount, self.mesh.obstacleCount, self.buildLatency or 0, self:timing('navmesh'), self.path and string.format('%d corners, %.0f long', #self.path, self.length) or 'none'))
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
end

function NavMesh:render()
    self:beginWorld()
    graphics2d.drawPolygon(kBoundary, '#FF263040')
    for _, rock in ipairs(self.rocks) do
        graphics2d.drawPolygon(rock, '#FF78909C', {layer = 2})
    end
    for _, crate in ipairs(self.crates) do
        graphics2d.drawPolygon(crate.points, '#FFA1887F', {layer = 2})
    end
    if self.mesh and self.showTriangles then
        for _, triangle in ipairs(self.mesh:triangles()) do
            graphics2d.drawPolyline(triangle, 1, '#50FFFFFF', true, {layer = 1})
        end
    end
    if self.path then
        graphics2d.drawPolyline(self.path, 4, '#FFFFD54F', false, {layer = 3})
    end
    graphics2d.drawCircle(self.agent.x, self.agent.y, math.max(self.radius, 6), '#AA4DD0E1', {layer = 4})
    if self.goal then
        graphics2d.drawRing(self.goal[1], self.goal[2], 12, 3, '#FFEF5350', {layer = 4})
    end
end

return NavMesh
