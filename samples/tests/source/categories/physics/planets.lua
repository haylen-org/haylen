-- Planets whose gravity is a radial force field with an inverse square falloff in a world without gravity, icy moons on circular orbits and rocket ships launched from the dock or the pointer that fall into orbit or crash.
local assets = require('haylen.assets')
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local input = require('haylen.input')
local m = require('haylen.math')
local physics2d = require('haylen.physics2d')
local timer = require('haylen.timer')
local ui = require('haylen.ui')

local Burst = require('categories.physics.burst')
local Look = require('categories.physics.look')
local PhysicsTest = require('categories.physics.physics-test')
local Scenery = require('categories.physics.scenery')

local Planets = haylen.class('Planets', PhysicsTest)

Planets.code = [[
local world = physics2d.newWorld({gravity = {0, 0}})
physics2d.newForceField(world, {kind = 'radial', x = -330, y = 40, radius = 560, strength = 1400, falloff = 'inverseSquare', minDistance = 120})
local speed = math.sqrt(1400 * 120 ^ 2 / distance)  -- The speed of a circular orbit.
local moon = world:createBody({x = -330, y = 40 - distance, vx = speed})]]

-- The gravity of each planet is the acceleration at its surface, and its moons are listed by orbit radius and size.
Planets.worlds = {
    {x = -330, y = 40, radius = 120, reach = 560, gravity = 1400, picture = 'physics/planet-grass.png', glow = '#594FA8E0', moons = {{220, 18}, {320, 14}}},
    {x = 440, y = -60, radius = 70, reach = 300, gravity = 900, picture = 'physics/planet-desert.png', glow = '#59F0A040', moons = {{160, 12}}},
}
-- The share of each picture of a planet that its disc covers.
Planets.cover = 376 / 384
Planets.dock = {-710, 40}
Planets.shipRadius = 9
Planets.shipSize = {48, 36.75}
Planets.nozzle = 21
Planets.maxShips = 8
Planets.trailEvery = 4
Planets.trailLength = 90
Planets.launchScale = 1.6
Planets.lostDistance = 1500
Planets.wreckTime = 1.2
Planets.secondShip = {delay = 1.5, speed = 1.18}

function Planets:enter()
    self:frame{
        code = Planets.code,
        hint = 'Drag from where a ship starts toward where it heads and let go to launch it. The dock launches at the speed of a circular orbit times the slider. R or the X button starts over.',
        controls = {
            ui.button{id = 'dock', text = 'Launch from the dock', onClick = function() self:launchFromDock(self.dockSpeed) end},
            ui.label{text = 'Dock speed in orbital speeds', font = 'caption', color = 'textMuted'},
            ui.slider{id = 'speed', value = 1, min = 0.5, max = 1.5, step = 0.05, showValue = true, onChange = function(event) self.dockSpeed = event.value end},
            ui.checkbox{id = 'fields', text = 'Show the fields', checked = true, onChange = function(event) self.showFields = event.checked end},
            ui.button{id = 'reset', text = 'Start over', onClick = function() self:build() end},
        },
        focus = 'dock',
    }
    self.dockSpeed, self.showFields = 1, true
    self.ship = assets.texture('physics/ship.png', {filter = 'linear'})
    self.moon = assets.texture('physics/planet-ice.png', {filter = 'linear'})
    self.pictures = {}
    for index, spec in ipairs(Planets.worlds) do
        self.pictures[index] = assets.texture(spec.picture, {filter = 'linear'})
    end
    self.glow = Scenery.glow()
    self.blasts = Burst('explosions/explosion-small', {count = 4, scale = 0.45, layer = 30})
    self.shipOptions = {width = Planets.shipSize[1], height = Planets.shipSize[2], rotation = 0, color = '#FFFFFFFF', layer = 3}
    self.trailStroke = {width = 1, color = '#66FFFFFF', join = 'round', cap = 'round', layer = 1}
    self:build()
end

function Planets:build()
    self.world = physics2d.newWorld({gravity = {0, 0}})
    self.planets, self.orbiters, self.ships = {}, {}, {}
    self.crashed, self.lost, self.steps = 0, 0, 0

    for index, spec in ipairs(Planets.worlds) do
        local body = self.world:createBody({type = 'static', x = spec.x, y = spec.y})
        body:addCircle(spec.radius)
        body.data = {planet = true}
        local field = physics2d.newForceField(self.world, {kind = 'radial', x = spec.x, y = spec.y, radius = spec.reach, strength = spec.gravity, falloff = 'inverseSquare', minDistance = spec.radius})
        self.planets[#self.planets + 1] = {body = body, field = field, spec = spec, picture = self.pictures[index]}
        for _, moon in ipairs(spec.moons) do
            self:orbiter(spec, -math.pi / 2, moon[1], moon[2], 1)
        end
    end

    self.world.onContactBegin = function(a, b)
        self:crash(a, b)
        self:crash(b, a)
    end
    self:launchFromDock(1)
    timer.after(Planets.secondShip.delay, function() self:launchFromDock(Planets.secondShip.speed) end, {owner = self})
end

-- A ship that touches anything but another ship crashes in a small blast and fades away.
function Planets:crash(body, other)
    local data = body.data
    if data.ship and not data.crashed and not other.data.ship then
        data.crashed, data.wreck = true, Planets.wreckTime
        self.crashed = self.crashed + 1
        self.blasts:fire(body.x, body.y)
    end
end

function Planets:body(x, y, vx, vy, radius)
    local body = self.world:createBody({x = x, y = y, vx = vx, vy = vy})
    body:addCircle(radius)
    body.data = {trail = {}, radius = radius}
    self.orbiters[#self.orbiters + 1] = body
    return body
end

-- A body at `distance` from the center of a planet, at `angle`, moving clockwise at `speed` times the speed of a circular orbit there.
function Planets:orbiter(spec, angle, distance, radius, speed)
    local cos, sin = math.cos(angle), math.sin(angle)
    local velocity = math.sqrt(spec.gravity * spec.radius ^ 2 / distance) * speed
    return self:body(spec.x + cos * distance, spec.y + sin * distance, -sin * velocity, cos * velocity, radius)
end

function Planets:launchFromDock(speed)
    local spec, dock = Planets.worlds[1], Planets.dock
    local distance = math.sqrt((dock[1] - spec.x) ^ 2 + (dock[2] - spec.y) ^ 2)
    self:addShip(self:orbiter(spec, math.atan(dock[2] - spec.y, dock[1] - spec.x), distance, Planets.shipRadius, speed))
end

function Planets:addShip(ship)
    ship.data.ship, ship.data.angle = true, 0
    self.ships[#self.ships + 1] = ship
    if #self.ships > Planets.maxShips then
        self:removeShip(1)
    end
end

function Planets:removeShip(index)
    local ship = table.remove(self.ships, index)
    for position, body in ipairs(self.orbiters) do
        if body == ship then
            table.remove(self.orbiters, position)
            break
        end
    end
    ship:destroy()
end

-- A press marks where the ship starts and the release launches it, faster the farther the pointer went, unless the start lies inside a planet.
function Planets:aim()
    local pointer = self.pointer
    if pointer.pressed and not ui.usingPointer() then
        self.start = {pointer.worldX, pointer.worldY}
        for _, planet in ipairs(self.planets) do
            local spec = planet.spec
            if (spec.x - pointer.worldX) ^ 2 + (spec.y - pointer.worldY) ^ 2 < (spec.radius + Planets.shipRadius * 2) ^ 2 then
                self.start = nil
            end
        end
    elseif pointer.released and self.start then
        local x, y = self.start[1], self.start[2]
        self:addShip(self:body(x, y, (pointer.worldX - x) * Planets.launchScale, (pointer.worldY - y) * Planets.launchScale, Planets.shipRadius))
        self.start = nil
    end
end

function Planets:update(dt)
    Planets.super.update(self, dt)
    if input.pressed('reset') then
        self:build()
    end
    self:aim()
    self.blasts:update(dt)
    local flying = 0
    for _, ship in ipairs(self.ships) do
        flying = flying + (ship.data.crashed and 0 or 1)
    end
    local moon = math.floor(self.orbiters[1].velocity:length() + 0.5)
    self:status(string.format('%d %s in flight, %d crashed and %d lost in space. The inner moon circles at %d units per second.', flying, flying == 1 and 'ship is' or 'ships are', self.crashed, self.lost, moon))
end

-- Trails keep a point every few steps, wrecks fade away, and ships far beyond the planets count as lost.
function Planets:fixedUpdate(step)
    self:simulate(self.world, step)
    self.steps = self.steps + 1
    for index = #self.ships, 1, -1 do
        local ship = self.ships[index]
        local data = ship.data
        if data.crashed then
            data.wreck = data.wreck - step
        end
        if data.crashed and data.wreck <= 0 then
            self:removeShip(index)
        elseif ship.x ^ 2 + ship.y ^ 2 > Planets.lostDistance ^ 2 then
            self.lost = self.lost + 1
            self:removeShip(index)
        end
    end
    if self.steps % Planets.trailEvery == 0 then
        for _, body in ipairs(self.orbiters) do
            local trail = body.data.trail
            trail[#trail + 1] = body.x
            trail[#trail + 1] = body.y
            if #trail > Planets.trailLength * 2 then
                table.remove(trail, 1)
                table.remove(trail, 1)
            end
        end
    end
end

-- A planet is its painted picture over a soft glow of its air, with the reach of its gravity as a faint disc.
function Planets:drawPlanet(planet)
    local spec = planet.spec
    if self.showFields then
        graphics2d.drawCircle(spec.x, spec.y, spec.reach, '#0FFFFFFF', {layer = -20})
        graphics2d.drawRing(spec.x, spec.y, spec.reach, 2 * Look.pixel, '#40FFFFFF', {layer = -20})
    end
    local air = spec.radius * 2.9
    graphics2d.draw(self.glow, spec.x, spec.y, {width = air, height = air, color = spec.glow, blend = 'additive', layer = -10})
    local size = spec.radius * 2 / Planets.cover
    graphics2d.draw(planet.picture, spec.x, spec.y, {width = size, height = size, layer = 0})
end

-- A ship points along its flight with a flickering flame behind its nozzle, and a wreck fades out.
function Planets:drawShip(ship)
    local data = ship.data
    local velocity = ship.velocity
    if not data.crashed and velocity:length() > 1 then
        data.angle = math.atan(velocity.y, velocity.x)
    end
    local angle = data.angle
    local cos, sin = math.cos(angle), math.sin(angle)
    local options = self.shipOptions
    options.rotation = angle
    options.color = data.crashed and {1, 0.6, 0.5, data.wreck / Planets.wreckTime} or '#FFFFFFFF'
    graphics2d.draw(self.ship, ship.x, ship.y, options)
    if data.crashed then
        return
    end
    local flicker = 0.8 + 0.2 * math.sin(haylen.elapsed() * 40 + ship.x)
    local length = 22 * flicker
    local x, y = ship.x - cos * (Planets.nozzle + length / 2), ship.y - sin * (Planets.nozzle + length / 2)
    graphics2d.drawShape({x - length / 2, y - 7, length, 14}, {kind = 'ellipse', rotation = angle, color = '#E6FF9A3C', layer = 2})
    graphics2d.drawShape({x - length * 0.3 + cos * 4, y - 4 + sin * 4, length * 0.6, 8}, {kind = 'ellipse', rotation = angle, color = '#FFFFF0A0', layer = 2})
end

function Planets:draw(area)
    Scenery.space()
    for _, planet in ipairs(self.planets) do
        self:drawPlanet(planet)
    end
    local stroke = self.trailStroke
    stroke.width = 3 * Look.pixel
    for _, body in ipairs(self.orbiters) do
        local data = body.data
        if #data.trail > 3 then
            stroke.color = data.ship and '#66FFE9A0' or '#40C9D3E2'
            graphics2d.drawStroke(data.trail, stroke)
        end
        if not data.ship then
            local size = data.radius * 2 / Planets.cover
            graphics2d.draw(self.moon, body.x, body.y, {width = size, height = size, layer = 1})
        end
    end
    for _, ship in ipairs(self.ships) do
        self:drawShip(ship)
    end
    self.blasts:draw()

    local dock = Planets.dock
    Look.disc(dock[1], dock[2], 20, 'steel', 1)
    graphics2d.drawRing(dock[1], dock[2], 30, 5, '#FF2FB3C4', {layer = 1})
    self:label('Dock', dock[1], dock[2] - 58, {size = 22})
    if self.start and self.pointer.down then
        Look.line(self.start[1], self.start[2], self.pointer.worldX, self.pointer.worldY, 5, '#FFFFC93C', 4)
        Look.pin(self.start[1], self.start[2], Planets.shipRadius, 4, '#FFFFC93C')
    end
end

return Planets
