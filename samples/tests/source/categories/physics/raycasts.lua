-- Every cast of a physics world aimed at the pointer: closest and all hits, filters, piercing, bounces, fans, shape casts, batched rays on the job system and picking with line of sight.
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local input = require('haylen.input')
local physics2d = require('haylen.physics2d')
local profiler = require('haylen.debug')
local ui = require('haylen.ui')

local parts = require('categories.physics.parts')
local PhysicsTest = require('categories.physics.physics-test')

local Raycasts = haylen.class('Raycasts', PhysicsTest)

local kWalls, kGlass, kCrates = 2, 4, 8
local kOrigin = {-680, 0}
local kLength = 1500
local kModes = {
    {id = 'closest', text = 'Closest hit'},
    {id = 'all', text = 'All hits'},
    {id = 'filter', text = 'Filter out the glass'},
    {id = 'pierce', text = 'Pierce three targets'},
    {id = 'bounce', text = 'Bounce like a laser'},
    {id = 'fan', text = 'Fan of rays'},
    {id = 'shapes', text = 'Shape casts'},
    {id = 'batch', text = 'Batch of 720 rays'},
    {id = 'pick', text = 'Pick and line of sight'},
}
local kRay = {layer = 30}

-- Each mode casts and returns what `render` draws: rays with their hits, extra points and a summary line.
local casts = {}

function Raycasts:enter()
    self:frame{
        hint = 'Aim with the mouse or a finger. Q and E or the shoulder buttons switch the cast. Blue walls, cyan glass and orange crates have their own categories.',
        controls = {
            ui.radioGroup{id = 'mode', items = kModes, selected = 'closest', onChange = function(event) self.mode = event.value end},
            ui.checkbox{id = 'engine', text = 'Engine ray drawing', onChange = function(event) self.world.debugRays = event.checked end},
        },
        focus = 'mode',
    }
    self.mode = 'closest'
    self.rays = physics2d.newRayBatch(720)
    self.drawn = {summary = ''}
    self:build()
end

function Raycasts:build()
    self.world = physics2d.newWorld({gravity = {0, 0}})
    self.bodies = {}
    local function add(kind, x, y, build, category, color)
        local body = self.world:createBody({type = kind, x = x, y = y})
        build(body, {category = category})
        parts.paint(body, color)
        self.bodies[#self.bodies + 1] = body
    end
    local wall, glass, crate = '#FF5C6BC0', '#9980DEEA', '#FFFFB74D'
    add('static', 0, -400, function(body, o) parts.box(body, 1500, 30, o) end, kWalls, wall)
    add('static', 0, 400, function(body, o) parts.box(body, 1500, 30, o) end, kWalls, wall)
    add('static', 760, 0, function(body, o) parts.box(body, 30, 830, o) end, kWalls, wall)
    add('static', 200, -150, function(body, o) parts.box(body, 40, 300, o) end, kWalls, wall)
    add('static', -200, 200, function(body, o) parts.polygon(body, {{-80, 60}, {0, -80}, {80, 60}}, o) end, kWalls, wall)
    add('static', 450, 170, function(body, o) parts.circle(body, 70, o) end, kWalls, wall)
    for index = 0, 2 do
        add('static', -350 + index * 130, -40, function(body, o) parts.box(body, 14, 200, o) end, kGlass, glass)
    end
    for index = 0, 3 do
        add('dynamic', 350 + index * 80, -60 + index * 50, function(body, o) parts.box(body, 50, 50, o) end, kCrates, crate)
    end
    add('dynamic', -500, -250, function(body, o) parts.capsule(body, -40, 0, 40, 0, 22, o) end, kCrates, crate)
end

function Raycasts:update(dt)
    Raycasts.super.update(self, dt)
    if input.pressed('next') or input.pressed('previous') then
        self:cycle(input.pressed('next') and 1 or -1)
    end
    self.target = {self.pointer.worldX, self.pointer.worldY}
    profiler.beginScope('casts')
    self.drawn = casts[self.mode](self)
    profiler.endScope()
    self:status(string.format('%s, casts %.3f ms', self.drawn.summary, self:timing('casts')))
end

function Raycasts:cycle(step)
    for index, mode in ipairs(kModes) do
        if mode.id == self.mode then
            self.mode = kModes[(index - 1 + step) % #kModes + 1].id
            self:set('mode', {selected = self.mode})
            return
        end
    end
end

function Raycasts:direction()
    local dx, dy = self.target[1] - kOrigin[1], self.target[2] - kOrigin[2]
    local length = math.max(math.sqrt(dx * dx + dy * dy), 1)
    return dx / length, dy / length
end

function Raycasts:ends()
    local dx, dy = self:direction()
    return kOrigin[1], kOrigin[2], kOrigin[1] + dx * kLength, kOrigin[2] + dy * kLength
end

function casts.closest(test)
    local x1, y1, x2, y2 = test:ends()
    local hit = test.world:raycast(x1, y1, x2, y2)
    return {rays = {{x1, y1, x2, y2, hit}}, summary = hit and string.format('Hit at %.0f units', hit.distance) or 'No hit'}
end

function casts.all(test)
    local x1, y1, x2, y2 = test:ends()
    local hits = test.world:raycastAll(x1, y1, x2, y2)
    return {rays = {{x1, y1, x2, y2}}, points = hits, summary = string.format('Hits in order %d', #hits)}
end

function casts.filter(test)
    local x1, y1, x2, y2 = test:ends()
    local blocked = test.world:raycast(x1, y1, x2, y2)
    local seeing = test.world:raycast(x1, y1 + 30, x2, y2 + 30, {mask = kWalls | kCrates, accept = function(hit) return hit.body.type ~= 'dynamic' or hit.distance > 200 end})
    return {rays = {{x1, y1, x2, y2, blocked}, {x1, y1 + 30, x2, y2 + 30, seeing}}, summary = 'Upper ray against every category, lower ray against walls and crates'}
end

function casts.pierce(test)
    local x1, y1, x2, y2 = test:ends()
    local hits = test.world:raycastAll(x1, y1, x2, y2, {limit = 3})
    local last = hits[#hits]
    return {rays = {{x1, y1, last and last.x or x2, last and last.y or y2}}, points = hits, summary = string.format('Pierced %d of 3', #hits)}
end

function casts.bounce(test)
    local dx, dy = test:direction()
    local hits, endX, endY = test.world:bounceRay(kOrigin[1], kOrigin[2], dx, dy, 4000, 8)
    local rays, x, y = {}, kOrigin[1], kOrigin[2]
    for _, hit in ipairs(hits) do
        rays[#rays + 1] = {x, y, hit.x, hit.y, hit}
        x, y = hit.x, hit.y
    end
    rays[#rays + 1] = {x, y, endX, endY}
    return {rays = rays, summary = string.format('Bounces %d', #hits)}
end

function casts.fan(test)
    local dx, dy = test:direction()
    local fan = test.world:rayFan(kOrigin[1], kOrigin[2], math.atan(dy, dx), math.rad(70), 31, 900)
    local rays, count = {}, 0
    for index, hit in ipairs(fan) do
        local angle = math.atan(dy, dx) + math.rad(70) * ((index - 1) / 30 - 0.5)
        rays[#rays + 1] = {kOrigin[1], kOrigin[2], kOrigin[1] + math.cos(angle) * 900, kOrigin[2] + math.sin(angle) * 900, hit or nil}
        count = count + (hit and 1 or 0)
    end
    return {rays = rays, summary = string.format('Rays that hit %d of 31', count)}
end

-- Four shapes swept from stacked starts toward the pointer, each drawn where it first touches.
function casts.shapes(test)
    local dx, dy = test:direction()
    local shapes, summary = {}, {}
    local function sweep(name, y, cast, outline)
        local tx, ty = dx * kLength, dy * kLength
        local hit = cast(y, tx, ty)
        local fraction = hit and hit.fraction or 1
        shapes[#shapes + 1] = {outline = outline, x = kOrigin[1] + tx * fraction, y = y + ty * fraction, from = y}
        summary[#summary + 1] = string.format('%s %s', name, hit and string.format('hits at %.0f', hit.distance) or 'clear')
    end
    local world, x = test.world, kOrigin[1]
    sweep('The circle', -150, function(y, tx, ty) return world:castCircle(x, y, 24, tx, ty) end, {radius = 24})
    sweep('the box', -50, function(y, tx, ty) return world:castBox(x, y, 48, 32, 0.3, tx, ty) end, {{-24, -16}, {24, -16}, {24, 16}, {-24, 16}, rotation = 0.3})
    sweep('the capsule', 50, function(y, tx, ty) return world:castCapsule(x - 20, y, x + 20, y, 16, tx, ty) end, {{-20, 0}, {20, 0}, capsule = 16})
    sweep('the polygon', 150, function(y, tx, ty) return world:castPolygon({{x, y - 26}, {x + 26, y + 20}, {x - 26, y + 20}}, tx, ty) end, {{0, -26}, {26, 20}, {-26, 20}})
    return {shapes = shapes, summary = table.concat(summary, ', ')}
end

-- A lidar of 720 rays around the pointer, cast together on the worker threads.
function casts.batch(test)
    local x, y = test.target[1], test.target[2]
    local rays = test.rays
    for index = 1, rays.size do
        local angle = index / rays.size * math.pi * 2
        rays:setRay(index, x, y, x + math.cos(angle) * 900, y + math.sin(angle) * 900)
    end
    test.world:raycastBatch(rays)
    local points, count = {}, 0
    for index = 1, rays.size do
        local hit, hx, hy = rays:hit(index)
        if hit then
            points[#points + 1] = {x = hx, y = hy}
            count = count + 1
        end
    end
    return {lidar = points, summary = string.format('Rays that hit %d of %d', count, rays.size)}
end

function casts.pick(test)
    local picked = test.world:pick(test.camera, test.pointer.x, test.pointer.y)
    local visible = test.world:lineOfSight(kOrigin[1], kOrigin[2], test.target[1], test.target[2], {mask = kWalls})
    local names = {}
    for _, shape in ipairs(picked) do
        names[#names + 1] = shape.body.type
    end
    return {picked = picked, sight = visible, summary = string.format('Under the pointer %s, line of sight %s', #names > 0 and '"' .. table.concat(names, '", "') .. '"' or 'nothing', visible and 'clear' or 'blocked')}
end

function Raycasts:fixedUpdate(step)
    self:simulate(self.world, step)
end

function Raycasts:drawShape(shape)
    local order = {layer = 31}
    local color = '#CCFFD54F'
    if shape.outline.radius then
        graphics2d.drawRing(shape.x, shape.y, shape.outline.radius, 3, color, order)
    elseif shape.outline.capsule then
        local radius = shape.outline.capsule
        graphics2d.drawLine(shape.x - 20, shape.y, shape.x + 20, shape.y, radius * 2, color, order)
    else
        local cos, sin = math.cos(shape.outline.rotation or 0), math.sin(shape.outline.rotation or 0)
        local points = {}
        for _, point in ipairs(shape.outline) do
            points[#points + 1] = {shape.x + point[1] * cos - point[2] * sin, shape.y + point[1] * sin + point[2] * cos}
        end
        graphics2d.drawPolyline(points, 3, color, true, order)
    end
    graphics2d.drawLine(kOrigin[1], shape.from, shape.x, shape.y, 2, '#66FFD54F', order)
end

function Raycasts:draw(area)
    parts.drawAll(self.bodies)
    graphics2d.drawCircle(kOrigin[1], kOrigin[2], 14, '#FFFFFFFF', kRay)
    local drawn = self.drawn
    if self.world.debugRays then
        self.world:debugDrawRays(kRay)
    else
        for _, ray in ipairs(drawn.rays or {}) do
            physics2d.drawRay(ray[1], ray[2], ray[3], ray[4], ray[5], kRay)
        end
    end
    for index, hit in ipairs(drawn.points or {}) do
        graphics2d.drawCircle(hit.x, hit.y, 9, '#FFFF5252', kRay)
        graphics2d.drawText(nil, tostring(index), hit.x + 10, hit.y - 30, {size = 22, layer = 31})
    end
    for _, shape in ipairs(drawn.shapes or {}) do
        self:drawShape(shape)
    end
    for _, point in ipairs(drawn.lidar or {}) do
        graphics2d.drawCircle(point.x, point.y, 3, '#FF69F0AE', kRay, 6)
    end
    for _, shape in ipairs(drawn.picked or {}) do
        graphics2d.drawRectOutline(shape.bounds, 4, '#FFFFFFFF', kRay)
    end
    if drawn.sight ~= nil then
        graphics2d.drawLine(kOrigin[1], kOrigin[2], self.target[1], self.target[2], 3, drawn.sight and '#FF69F0AE' or '#FFFF5252', kRay)
    end
end

return Raycasts
