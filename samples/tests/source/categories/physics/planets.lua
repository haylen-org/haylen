-- Planets whose gravity is a radial force field with an inverse square falloff in a world without gravity, moons on circular orbits and ships launched from the pointer that fall into orbit or crash.
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local input = require('haylen.input')
local m = require('haylen.math')
local physics2d = require('haylen.physics2d')
local ui = require('haylen.ui')

local parts = require('categories.physics.parts')
local PhysicsTest = require('categories.physics.physics-test')

local Planets = haylen.class('Planets', PhysicsTest)

-- The gravity of each planet is the acceleration at its surface, and its moons are listed by orbit radius and size.
local kPlanets = {
    {x = -330, y = 40, radius = 120, reach = 560, gravity = 1400, color = '#FF4F86C6', moons = {{220, 18}, {320, 14}}},
    {x = 440, y = -60, radius = 70, reach = 300, gravity = 900, color = '#FFC7845B', moons = {{160, 12}}},
}
local kDock = {-710, 40}
local kShipRadius = 9
local kMaxShips = 8
local kTrailEvery = 4
local kTrailLength = 90
local kLaunchScale = 1.6
local kLostDistance = 1500
local kWreckTime = 3

function Planets:enter()
    self:frame{
        hint = 'Drag from where a ship starts toward where it heads and let go to launch it. The dock launches at the speed of a circular orbit times the slider. R or the X button starts over.',
        controls = {
            ui.button{id = 'dock', text = 'Launch from the dock', onClick = function() self:launchFromDock() end},
            ui.label{text = 'Dock speed in orbital speeds', font = 'caption', color = 'textMuted'},
            ui.slider{id = 'speed', value = 1, min = 0.5, max = 1.5, step = 0.05, showValue = true, onChange = function(event) self.dockSpeed = event.value end},
            ui.checkbox{id = 'fields', text = 'Show the fields', checked = true, onChange = function(event) self.showFields = event.checked end},
            ui.button{id = 'reset', text = 'Start over', onClick = function() self:build() end},
        },
        focus = 'dock',
    }
    self.dockSpeed, self.showFields = 1, true
    local random = m.random(49)
    self.stars = {}
    for index = 1, 160 do
        self.stars[index] = {random:range(-800, 800), random:range(-430, 430), random:range(1, 2.5)}
    end
    self:build()
end

function Planets:build()
    self.world = physics2d.newWorld({gravity = {0, 0}})
    self.planets, self.orbiters, self.ships = {}, {}, {}
    self.crashed, self.lost, self.steps = 0, 0, 0

    for _, spec in ipairs(kPlanets) do
        local body = self.world:createBody({type = 'static', x = spec.x, y = spec.y})
        parts.circle(body, spec.radius)
        parts.paint(body, spec.color)
        body.data.planet = true
        local field = physics2d.newForceField(self.world, {kind = 'radial', x = spec.x, y = spec.y, radius = spec.reach, strength = spec.gravity, falloff = 'inverseSquare', minDistance = spec.radius})
        self.planets[#self.planets + 1] = {body = body, field = field, spec = spec}
        for _, moon in ipairs(spec.moons) do
            self:orbiter(spec, -math.pi / 2, moon[1], moon[2], 1)
        end
    end

    self.world.onContactBegin = function(a, b)
        for _, pair in ipairs({{a, b}, {b, a}}) do
            local data = pair[1].data
            if data.ship and not data.crashed and not pair[2].data.ship then
                data.crashed, data.wreck = true, kWreckTime
                self.crashed = self.crashed + 1
            end
        end
    end
    self:launchFromDock()
end

function Planets:body(x, y, vx, vy, radius)
    local body = self.world:createBody({x = x, y = y, vx = vx, vy = vy})
    parts.circle(body, radius)
    body.data.trail = {}
    self.orbiters[#self.orbiters + 1] = body
    return body
end

-- A body at `distance` from the center of a planet, at `angle`, moving clockwise at `speed` times the speed of a circular orbit there.
function Planets:orbiter(spec, angle, distance, radius, speed)
    local cos, sin = math.cos(angle), math.sin(angle)
    local velocity = math.sqrt(spec.gravity * spec.radius ^ 2 / distance) * speed
    return self:body(spec.x + cos * distance, spec.y + sin * distance, -sin * velocity, cos * velocity, radius)
end

function Planets:launchFromDock()
    local spec = kPlanets[1]
    local distance = math.sqrt((kDock[1] - spec.x) ^ 2 + (kDock[2] - spec.y) ^ 2)
    self:addShip(self:orbiter(spec, math.atan(kDock[2] - spec.y, kDock[1] - spec.x), distance, kShipRadius, self.dockSpeed))
end

function Planets:addShip(ship)
    parts.paint(ship, '#FFFFFFFF')
    ship.data.ship = true
    self.ships[#self.ships + 1] = ship
    if #self.ships > kMaxShips then
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
    if pointer.pressed then
        self.start = {pointer.worldX, pointer.worldY}
        for _, planet in ipairs(self.planets) do
            local spec = planet.spec
            if (spec.x - pointer.worldX) ^ 2 + (spec.y - pointer.worldY) ^ 2 < (spec.radius + kShipRadius * 2) ^ 2 then
                self.start = nil
            end
        end
    elseif pointer.released and self.start then
        local x, y = self.start[1], self.start[2]
        self:addShip(self:body(x, y, (pointer.worldX - x) * kLaunchScale, (pointer.worldY - y) * kLaunchScale, kShipRadius))
        self.start = nil
    end
end

function Planets:update(dt)
    Planets.super.update(self, dt)
    if input.pressed('reset') then
        self:build()
    end
    self:aim()
    local flying = 0
    for _, ship in ipairs(self.ships) do
        flying = flying + (ship.data.crashed and 0 or 1)
    end
    local moon = self.orbiters[1].velocity
    self:status(string.format('Ships flying %d, crashed %d, lost in space %d, inner moon at %.0f units per second, step %.2f ms', flying, self.crashed, self.lost, math.sqrt(moon.x ^ 2 + moon.y ^ 2), self:stepTime()))
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
        elseif ship.x ^ 2 + ship.y ^ 2 > kLostDistance ^ 2 then
            self.lost = self.lost + 1
            self:removeShip(index)
        end
    end
    if self.steps % kTrailEvery == 0 then
        for _, body in ipairs(self.orbiters) do
            local trail = body.data.trail
            trail[#trail + 1] = {body.x, body.y}
            if #trail > kTrailLength then
                table.remove(trail, 1)
            end
        end
    end
end

function Planets:draw(area)
    for _, star in ipairs(self.stars) do
        graphics2d.drawCircle(star[1], star[2], star[3], '#88FFFFFF', {layer = -2})
    end
    for _, planet in ipairs(self.planets) do
        local spec = planet.spec
        if self.showFields then
            graphics2d.drawCircle(spec.x, spec.y, spec.reach, m.color(spec.color):withAlpha(0.06), {layer = -1})
            graphics2d.drawRing(spec.x, spec.y, spec.reach, 2, m.color(spec.color):withAlpha(0.3), {layer = -1})
        end
        graphics2d.drawRing(spec.x, spec.y, spec.radius + 8, 10, m.color(spec.color):withAlpha(0.35), {layer = 1})
        parts.draw(planet.body, {layer = 1})
    end
    for _, body in ipairs(self.orbiters) do
        local data = body.data
        if #data.trail > 1 then
            graphics2d.drawPolyline(data.trail, 2, data.ship and '#88FFFFFF' or '#5590A4AE', false)
        end
        if not data.ship then
            parts.draw(body, {layer = 2})
        end
    end
    for _, ship in ipairs(self.ships) do
        local velocity = ship.velocity
        local angle = math.atan(velocity.y, velocity.x)
        local cos, sin = math.cos(angle), math.sin(angle)
        local x, y = ship.x, ship.y
        local color = ship.data.crashed and m.color(1, 0.45, 0.4, ship.data.wreck / kWreckTime) or '#FFFFFFFF'
        graphics2d.drawPolygon({{x + cos * 16, y + sin * 16}, {x - cos * 10 - sin * 10, y - sin * 10 + cos * 10}, {x - cos * 10 + sin * 10, y - sin * 10 - cos * 10}}, color, {layer = 3})
    end
    graphics2d.drawCircle(kDock[1], kDock[2], 14, '#FF8FB0FF', {layer = 1})
    Planets.caption('Dock', kDock[1], kDock[2] + 22, {anchor = {0.5, 0}})
    if self.start and self.pointer.down then
        graphics2d.drawLine(self.start[1], self.start[2], self.pointer.worldX, self.pointer.worldY, 3, '#FFFFD54F', {layer = 4})
        graphics2d.drawCircle(self.start[1], self.start[2], kShipRadius, '#FFFFD54F', {layer = 4})
    end
end

return Planets
