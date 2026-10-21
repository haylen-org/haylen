-- Every cast of a physics world aimed from a laser at the pointer: closest and all hits, filters, piercing, bounces, fans, shape casts, batched rays on the job system and picking with line of sight. Until the pointer comes over the room, the laser sweeps across it by itself.
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local input = require('haylen.input')
local physics2d = require('haylen.physics2d')
local profiler = require('haylen.debug')
local ui = require('haylen.ui')

local Look = require('categories.physics.look')
local PhysicsTest = require('categories.physics.physics-test')
local Scenery = require('categories.physics.scenery')

local Raycasts = haylen.class('Raycasts', PhysicsTest)

Raycasts.code = [[
local hit = world:raycast(x1, y1, x2, y2, {mask = kWalls | kCrates})
local pierced = world:raycastAll(x1, y1, x2, y2, {limit = 3})
local bounces, endX, endY = world:bounceRay(x, y, dx, dy, 4000, 8)
local fan = world:rayFan(x, y, angle, math.rad(70), 31, 900)
local swept = world:castCircle(x, y, 24, dx * 1500, dy * 1500)
world:raycastBatch(rays)
local visible = world:lineOfSight(x1, y1, x2, y2, {mask = kWalls})]]

Raycasts.walls, Raycasts.glass, Raycasts.crates = 2, 4, 8
Raycasts.names = {[2] = 'a wall', [4] = 'a glass pane', [8] = 'a crate'}
Raycasts.origin = {-660, 0}
Raycasts.length = 1500
Raycasts.modes = {
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
Raycasts.laser = '#FFFF5A4E'
Raycasts.hitColor = '#FFFFC93C'

-- Each mode casts and returns what `draw` shows: rays with their hits, extra points and a summary sentence.
local casts = {}

function Raycasts:enter()
    self:frame{
        code = Raycasts.code,
        hint = 'Aim with the mouse or a finger. Q and E or the shoulder buttons switch the cast. Stone walls, glass panes and crates have their own categories.',
        controls = {
            ui.radioGroup{id = 'mode', items = Raycasts.modes, selected = 'closest', onChange = function(event) self.mode = event.value end},
            ui.checkbox{id = 'engine', text = 'Draw the rays with the engine', onChange = function(event) self.world.debugRays = event.checked end},
        },
        focus = 'mode',
    }
    self.mode = 'closest'
    self.rays = physics2d.newRayBatch(720)
    self.drawn = {summary = ''}
    self.time = 0
    self:build()
end

function Raycasts:build()
    self.world = physics2d.newWorld({gravity = {0, 0}})
    self.bodies = {}
    local function add(kind, x, y, build, category, material)
        local body = self.world:createBody({type = kind, x = x, y = y})
        build(body, {category = category})
        Look.dress(body, material)
        self.bodies[#self.bodies + 1] = body
    end
    local walls, glass, crates = Raycasts.walls, Raycasts.glass, Raycasts.crates
    add('static', 0, -330, function(body, o) body:addBox(1560, 30, o) end, walls, 'slate')
    add('static', 0, 330, function(body, o) body:addBox(1560, 30, o) end, walls, 'slate')
    add('static', 765, 0, function(body, o) body:addBox(30, 690, o) end, walls, 'slate')
    add('static', -765, 0, function(body, o) body:addBox(30, 690, o) end, walls, 'slate')
    add('static', 200, -165, function(body, o) body:addBox(40, 300, o) end, walls, 'stone')
    add('static', -200, 200, function(body, o) body:addPolygon({{-80, 60}, {0, -80}, {80, 60}}, o) end, walls, 'stone')
    add('static', 450, 150, function(body, o) body:addCircle(70, o) end, walls, 'stone')
    for index = 0, 2 do
        add('static', -350 + index * 130, -40, function(body, o) body:addBox(16, 200, o) end, glass, 'glass')
    end
    for index = 0, 3 do
        add('dynamic', 350 + index * 80, -60 + index * 50, function(body, o) body:addBox(50, 50, o) end, crates, 'woodCrate')
    end
    add('dynamic', -500, -230, function(body, o) body:addCapsule(-40, 0, 40, 0, 22, o) end, crates, 'wood')
end

function Raycasts:update(dt)
    Raycasts.super.update(self, dt)
    self.time = self.time + dt
    if input.pressed('next') or input.pressed('previous') then
        self:cycle(input.pressed('next') and 1 or -1)
    end
    -- The laser sweeps by itself until the pointer comes over the room.
    local stage = self.stage
    if not self.aimed and stage and stage:contains({self.pointer.x, self.pointer.y}) then
        self.aimed = true
    end
    if self.aimed then
        self.target = {self.pointer.worldX, self.pointer.worldY}
    else
        local angle = math.sin(self.time * 0.5) * 0.55
        self.target = {Raycasts.origin[1] + math.cos(angle) * 900, Raycasts.origin[2] + math.sin(angle) * 900}
    end
    profiler.beginScope('casts')
    self.drawn = casts[self.mode](self)
    profiler.endScope()
    self:status(self.drawn.summary)
end

function Raycasts:cycle(step)
    for index, mode in ipairs(Raycasts.modes) do
        if mode.id == self.mode then
            self.mode = Raycasts.modes[(index - 1 + step) % #Raycasts.modes + 1].id
            self:set('mode', {selected = self.mode})
            return
        end
    end
end

function Raycasts:direction()
    local origin = Raycasts.origin
    local dx, dy = self.target[1] - origin[1], self.target[2] - origin[2]
    local length = math.max(math.sqrt(dx * dx + dy * dy), 1)
    return dx / length, dy / length
end

function Raycasts:ends()
    local origin = Raycasts.origin
    local dx, dy = self:direction()
    return origin[1], origin[2], origin[1] + dx * Raycasts.length, origin[2] + dy * Raycasts.length
end

function Raycasts.nameOf(hit)
    return Raycasts.names[hit.shape.category] or 'a body'
end

function casts.closest(test)
    local x1, y1, x2, y2 = test:ends()
    local hit = test.world:raycast(x1, y1, x2, y2)
    return {rays = {{x1, y1, x2, y2, hit}}, summary = hit and string.format('The ray hits %s %d units away.', Raycasts.nameOf(hit), math.floor(hit.distance + 0.5)) or 'The ray hits nothing.'}
end

function casts.all(test)
    local x1, y1, x2, y2 = test:ends()
    local hits = test.world:raycastAll(x1, y1, x2, y2)
    return {rays = {{x1, y1, x2, y2}}, points = hits, summary = string.format('The ray crosses %d shapes, numbered from the nearest.', #hits)}
end

function casts.filter(test)
    local x1, y1, x2, y2 = test:ends()
    local blocked = test.world:raycast(x1, y1, x2, y2)
    local walls, crates = Raycasts.walls, Raycasts.crates
    local seeing = test.world:raycast(x1, y1 + 30, x2, y2 + 30, {mask = walls | crates, accept = function(hit) return hit.body.type ~= 'dynamic' or hit.distance > 200 end})
    return {rays = {{x1, y1, x2, y2, blocked}, {x1, y1 + 30, x2, y2 + 30, seeing}}, summary = 'The upper ray stops at anything, and the lower one passes the glass and the crates near the laser.'}
end

function casts.pierce(test)
    local x1, y1, x2, y2 = test:ends()
    local hits = test.world:raycastAll(x1, y1, x2, y2, {limit = 3})
    local last = hits[#hits]
    return {rays = {{x1, y1, last and last.x or x2, last and last.y or y2}}, points = hits, summary = string.format('The ray pierces %d of 3 targets.', #hits)}
end

function casts.bounce(test)
    local origin = Raycasts.origin
    local dx, dy = test:direction()
    local hits, endX, endY = test.world:bounceRay(origin[1], origin[2], dx, dy, 4000, 8)
    local rays, x, y = {}, origin[1], origin[2]
    for _, hit in ipairs(hits) do
        rays[#rays + 1] = {x, y, hit.x, hit.y, hit}
        x, y = hit.x, hit.y
    end
    rays[#rays + 1] = {x, y, endX, endY}
    return {rays = rays, summary = string.format('The laser bounces %d times.', #hits)}
end

function casts.fan(test)
    local origin = Raycasts.origin
    local dx, dy = test:direction()
    local fan = test.world:rayFan(origin[1], origin[2], math.atan(dy, dx), math.rad(70), 31, 900)
    local rays, count = {}, 0
    for index, hit in ipairs(fan) do
        local angle = math.atan(dy, dx) + math.rad(70) * ((index - 1) / 30 - 0.5)
        rays[#rays + 1] = {origin[1], origin[2], origin[1] + math.cos(angle) * 900, origin[2] + math.sin(angle) * 900, hit or nil}
        count = count + (hit and 1 or 0)
    end
    return {rays = rays, thin = true, summary = string.format('%d of 31 rays of the fan hit something.', count)}
end

-- Four shapes swept from stacked starts toward the pointer, each drawn where it first touches.
function casts.shapes(test)
    local origin = Raycasts.origin
    local dx, dy = test:direction()
    local shapes, summary = {}, {}
    local function sweep(name, y, cast, outline)
        local tx, ty = dx * Raycasts.length, dy * Raycasts.length
        local hit = cast(y, tx, ty)
        local fraction = hit and hit.fraction or 1
        shapes[#shapes + 1] = {outline = outline, x = origin[1] + tx * fraction, y = y + ty * fraction, from = y}
        summary[#summary + 1] = string.format('the %s %s', name, hit and string.format('after %d', math.floor(hit.distance + 0.5)) or 'never')
    end
    local world, x = test.world, origin[1]
    sweep('circle', -150, function(y, tx, ty) return world:castCircle(x, y, 24, tx, ty) end, {radius = 24})
    sweep('box', -50, function(y, tx, ty) return world:castBox(x, y, 48, 32, 0.3, tx, ty) end, {{-24, -16}, {24, -16}, {24, 16}, {-24, 16}, rotation = 0.3})
    sweep('capsule', 50, function(y, tx, ty) return world:castCapsule(x - 20, y, x + 20, y, 16, tx, ty) end, {{-20, 0}, {20, 0}, capsule = 16})
    sweep('triangle', 150, function(y, tx, ty) return world:castPolygon({{x, y - 26}, {x + 26, y + 20}, {x - 26, y + 20}}, tx, ty) end, {{0, -26}, {26, 20}, {-26, 20}})
    local text = table.concat(summary, ', ')
    return {shapes = shapes, summary = 'Swept toward the pointer, ' .. text .. ' units.'}
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
    return {lidar = points, summary = string.format('%d of %d rays around the pointer hit something.', count, rays.size)}
end

function casts.pick(test)
    local origin = Raycasts.origin
    local picked = test.world:pick(test.camera, test.pointer.x, test.pointer.y)
    local visible = test.world:lineOfSight(origin[1], origin[2], test.target[1], test.target[2], {mask = Raycasts.walls})
    local names = {}
    for _, shape in ipairs(picked) do
        names[#names + 1] = Raycasts.names[shape.category] or 'a body'
    end
    return {picked = picked, sight = visible, summary = string.format('Under the pointer: %s. The laser %s.', #names > 0 and table.concat(names, ', ') or 'nothing', visible and 'sees the pointer' or 'has a wall between it and the pointer')}
end

function Raycasts:fixedUpdate(step)
    self:simulate(self.world, step)
end

-- A beam is a glowing line with the bold outline, and a hit is a ring with the normal of the surface as a short line.
function Raycasts:drawBeam(x1, y1, x2, y2, hit, thin)
    local unit = Look.pixel
    local width = (thin and 3 or 5) * unit
    if hit then
        x2, y2 = hit.x, hit.y
    end
    Look.line(x1, y1, x2, y2, width, Raycasts.laser, 30)
    graphics2d.drawLine(x1, y1, x2, y2, width * 3, '#40FF5A4E', {layer = 29, blend = 'additive'})
    if hit then
        Look.line(hit.x, hit.y, hit.x + hit.normalX * 40, hit.y + hit.normalY * 40, 3 * unit, '#FFFFFFFF', 31)
        Look.pin(hit.x, hit.y, 7 * unit, 32, Raycasts.hitColor)
    end
end

function Raycasts:drawShape(shape)
    local unit = Look.pixel
    local color = '#CCFFC93C'
    local order = {layer = 31}
    Look.dashed(Raycasts.origin[1], shape.from, shape.x, shape.y, '#99FFC93C', 30)
    if shape.outline.radius then
        graphics2d.drawRing(shape.x, shape.y, shape.outline.radius, 4 * unit, color, order)
    elseif shape.outline.capsule then
        local radius = shape.outline.capsule
        graphics2d.drawShape({shape.x - 20 - radius, shape.y - radius, 40 + radius * 2, radius * 2}, {radius = radius, color = '#00000000', borderWidth = 4 * unit, borderColor = color, layer = 31})
    else
        local cos, sin = math.cos(shape.outline.rotation or 0), math.sin(shape.outline.rotation or 0)
        local points = {}
        for _, point in ipairs(shape.outline) do
            points[#points + 1] = shape.x + point[1] * cos - point[2] * sin
            points[#points + 1] = shape.y + point[1] * sin + point[2] * cos
        end
        graphics2d.drawStroke(points, {width = 4 * unit, color = color, closed = true, join = 'round', layer = 31})
    end
end

-- The laser is a small turret on the wall with a red lens.
function Raycasts:drawLaser()
    local origin = Raycasts.origin
    local unit = Look.pixel
    Look.block(origin[1] - 50, origin[2], 70, 40, 'iron', 20)
    graphics2d.drawCircle(origin[1], origin[2], 14, Raycasts.laser, {layer = 21})
    graphics2d.drawRing(origin[1], origin[2], 14, 3.5 * unit, Look.ink, {layer = 21})
    graphics2d.drawCircle(origin[1] - 4, origin[2] - 4, 4, '#FFFFD6D0', {layer = 21})
end

function Raycasts:draw(area)
    Scenery.sky(self.camera, 330)
    graphics2d.drawRect({-750, -315, 1500, 630}, '#FF3A3F66', {layer = -20})
    local unit = Look.pixel
    for x = -700, 700, 100 do
        graphics2d.drawLine(x, -315, x, 315, 2 * unit, '#FF444A74', {layer = -19})
    end
    for y = -300, 300, 100 do
        graphics2d.drawLine(-750, y, 750, y, 2 * unit, '#FF444A74', {layer = -19})
    end
    Look.drawAll(self.bodies)
    self:drawLaser()
    local drawn = self.drawn
    if self.world.debugRays then
        self.world:debugDrawRays({layer = 30})
    else
        for _, ray in ipairs(drawn.rays or {}) do
            self:drawBeam(ray[1], ray[2], ray[3], ray[4], ray[5], drawn.thin)
        end
    end
    for index, hit in ipairs(drawn.points or {}) do
        Look.pin(hit.x, hit.y, 9 * Look.pixel, 32, Raycasts.hitColor)
        self:tag(tostring(index), hit.x + 16, hit.y - 22, {size = 20})
    end
    for _, shape in ipairs(drawn.shapes or {}) do
        self:drawShape(shape)
    end
    for _, point in ipairs(drawn.lidar or {}) do
        graphics2d.drawCircle(point.x, point.y, 4 * unit, '#FF4CD45A', {layer = 31})
    end
    for _, shape in ipairs(drawn.picked or {}) do
        local bounds = shape.bounds
        graphics2d.drawShape({bounds.x - 6, bounds.y - 6, bounds.width + 12, bounds.height + 12}, {radius = 8, color = '#00000000', borderWidth = 4 * unit, borderColor = Raycasts.hitColor, layer = 31})
    end
    if drawn.sight ~= nil then
        Look.line(Raycasts.origin[1], Raycasts.origin[2], self.target[1], self.target[2], 4 * unit, drawn.sight and '#FF4CD45A' or Raycasts.laser, 30)
    end
end

return Raycasts
