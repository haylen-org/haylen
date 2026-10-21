-- Two worlds: two planets in space, each in a physics world of its own without gravity, whose pull is a radial force field of constant strength inside a bubble around it, so a body falls toward the planet whose bubble holds it and flies in a straight line between the bubbles. The near planet is grassy with a slingshot on top, and the far one is an icy moon with a stronger pull, a thin atmosphere that slows what flies in and slippery towers of ice with critters on them. A shot that enters the bubble of the other planet moves to its world with `body:moveToWorld`, keeping its speed and spin, and hits what stands there.
local assets = require('haylen.assets')
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local input = require('haylen.input')
local m = require('haylen.math')
local physics2d = require('haylen.physics2d')
local ui = require('haylen.ui')

local Burst = require('categories.physics.burst')
local Look = require('categories.physics.look')
local PhysicsTest = require('categories.physics.physics-test')
local Scenery = require('categories.physics.scenery')

local TwoWorlds = haylen.class('TwoWorlds', PhysicsTest)

TwoWorlds.code = [[
local home = physics2d.newWorld({gravity = {0, 0}})  -- Each planet has a world of its own.
physics2d.newForceField(home, {kind = 'radial', x = -470, y = 120, radius = 310, strength = 520})  -- A bubble of constant pull.
physics2d.newForceField(moon, {kind = 'radial', x = 430, y = -30, radius = 330, strength = 820, linearDrag = 0.35})
if shot.world == home and insideBubble(moonPlanet, shot) then shot = shot:moveToWorld(moon) end]]

TwoWorlds.actions = {
    {name = 'aim', type = 'vector', up = {'key:w', 'key:up', 'button:dpadUp'}, down = {'key:s', 'key:down', 'button:dpadDown'}, left = {'key:a', 'key:left', 'button:dpadLeft'}, right = {'key:d', 'key:right', 'button:dpadRight'}, bindings = {'stick:left', 'virtualStick:aim'}},
    {name = 'launch', type = 'button', bindings = {'key:space', 'button:south', 'virtual:launch'}},
}

-- Each planet: its center, its radius, the reach of its bubble, its pull, the drag of its air, the friction of what stands on it, its picture and the color of its bubble.
TwoWorlds.planets = {
    home = {x = -470, y = 120, radius = 110, reach = 310, pull = 520, drag = 0, friction = 0.8, picture = 'physics/planet-grass.png', rim = '#FF7BC86A', name = 'Home'},
    moon = {x = 430, y = -30, radius = 120, reach = 330, pull = 820, drag = 0.35, friction = 0.08, picture = 'physics/planet-ice.png', rim = '#FFD8ECFA', name = 'Ice moon'},
}
-- The share of each picture of a planet that its disc covers.
TwoWorlds.cover = 376 / 384
-- The slingshot starts pulled back so its predicted flight crosses to the moon and lands among its towers.
TwoWorlds.firstPull = {-100, 46}
-- The towers on the moon: the angle around it where each stands and the blocks it stacks from the ground up, as width and height.
TwoWorlds.towers = {
    {angle = math.pi, blocks = {{24, 70}, {90, 20}, {24, 50}}},
    {angle = math.pi * 0.78, blocks = {{60, 40}, {40, 40}}},
    {angle = math.pi * 1.22, blocks = {{20, 80}, {80, 18}}},
}
TwoWorlds.pouchHeight = 70
TwoWorlds.maxPull = 120
TwoWorlds.power = 5.2
TwoWorlds.shotRadius = 16
TwoWorlds.maxShots = 6
TwoWorlds.popSpeed = 180
TwoWorlds.lostDistance = 1400
TwoWorlds.predictSteps = 150
TwoWorlds.trailLength = 240

function TwoWorlds:enter()
    self:frame{
        code = TwoWorlds.code,
        hint = 'Drag the shot back from the slingshot on the near planet and let go, or aim with the arrows, WASD or the left stick and launch with Space or the south button. The dotted line is the predicted flight through both bubbles. R or the X button builds the planets again.',
        controls = {
            ui.button{id = 'launch', text = 'Launch', variant = 'primary', onClick = function() self:launch() end},
            ui.formField{label = 'Pull of the ice moon', ui.slider{id = 'pull', min = 300, max = 1400, step = 20, value = TwoWorlds.planets.moon.pull, showValue = true, decimals = 0, onChange = function(event)
                self.moon.field.strength = event.value
                self.moon.pull = event.value
            end}},
            ui.checkbox{id = 'path', text = 'Show the predicted flight', checked = true, onChange = function(event) self.showPath = event.checked end},
            ui.button{id = 'reset', text = 'Build the planets again', onClick = function() self:build() end},
        },
        play = true,
        actions = TwoWorlds.actions,
        view = {1900, 880},
        overlay = {
            ui.touchStick{action = 'aim', radius = 110, mode = 'floating', touchOnly = true, anchor = 'bottomLeft', anchorTo = 'parent', margin = {0, 0, 24, 24}, width = 360, height = 300},
            ui.touchButton{action = 'launch', text = 'Launch', size = 150, touchOnly = true, anchor = 'bottomRight', anchorTo = 'parent', margin = {0, 24, 24, 0}},
        },
    }
    self.camera.position = {0, 35}
    self.showPath = true
    self.pictures = {home = assets.texture(TwoWorlds.planets.home.picture, {filter = 'linear'}), moon = assets.texture(TwoWorlds.planets.moon.picture, {filter = 'linear'})}
    self.poofs = Burst('explosions/poof', {count = 4, scale = 0.6, layer = 30})
    self.dots = {}
    self.trailStroke = {width = 1, color = '#66FFFFFF', join = 'round', cap = 'round', layer = 1}
    self.band = {0, 0, 0, 0, 0, 0}
    self.bandStroke = {width = 1, color = '#FF5D3A22', join = 'round', cap = 'round', layer = 3}
    self:build()
end

-- Makes a world for a planet with the planet itself and the bubble of its pull.
function TwoWorlds.makePlanet(spec)
    local planet = {spec = spec, pull = spec.pull}
    planet.world = physics2d.newWorld({gravity = {0, 0}})
    planet.body = planet.world:createBody({type = 'static', x = spec.x, y = spec.y})
    planet.body:addCircle(spec.radius, {friction = spec.friction})
    planet.field = physics2d.newForceField(planet.world, {kind = 'radial', x = spec.x, y = spec.y, radius = spec.reach, strength = spec.pull, linearDrag = spec.drag})
    return planet
end

function TwoWorlds:build()
    self.home = TwoWorlds.makePlanet(TwoWorlds.planets.home)
    self.moon = TwoWorlds.makePlanet(TwoWorlds.planets.moon)
    self.bodies, self.shots = {}, {}
    self.critters, self.popped, self.launched, self.crossings = 0, 0, 0, 0

    -- The towers stand on the moon square to its surface, so its pull holds them up.
    local moon = self.moon.spec
    for _, tower in ipairs(TwoWorlds.towers) do
        local cos, sin = math.cos(tower.angle), math.sin(tower.angle)
        local height = moon.radius
        for _, block in ipairs(tower.blocks) do
            height = height + block[2] / 2
            local body = self.moon.world:createBody({x = moon.x + cos * height, y = moon.y + sin * height, rotation = tower.angle + math.pi / 2})
            body:addBox(block[1], block[2], {friction = moon.friction, density = 0.6})
            Look.dress(body, 'ice')
            self.bodies[#self.bodies + 1] = body
            height = height + block[2] / 2
        end
        local critter = self.moon.world:createBody({x = moon.x + cos * (height + 18), y = moon.y + sin * (height + 18)})
        critter:addCircle(18, {friction = moon.friction, density = 0.4})
        critter.data = {critter = true}
        self.bodies[#self.bodies + 1] = critter
        self.critters = self.critters + 1
    end

    -- A hard hit pops a critter in either world, since a critter that flies off may land on the other planet.
    for _, planet in ipairs({self.home, self.moon}) do
        planet.world.onHit = function(a, b, contact)
            if contact.speed >= TwoWorlds.popSpeed then
                self:pop(a)
                self:pop(b)
            end
        end
    end
    self.pull = m.vec2(TwoWorlds.firstPull[1], TwoWorlds.firstPull[2])
    self:loadShot()
end

-- Returns where the pouch of the slingshot rests, over the top of the near planet.
function TwoWorlds:pouch()
    local home = self.home.spec
    return home.x, home.y - home.radius - TwoWorlds.pouchHeight
end

function TwoWorlds:loadShot()
    local x, y = self:pouch()
    self.shot = self.home.world:createBody({type = 'kinematic', x = x + self.pull.x, y = y + self.pull.y})
    self.shot:addCircle(TwoWorlds.shotRadius, {density = 3, restitution = 0.2, friction = 0.6})
    Look.dress(self.shot, 'red', {spin = true})
    self.shot.data.shot = true
end

function TwoWorlds:launch()
    if not self.shot or self.pull:length() < 15 then
        return
    end
    local velocity = self.pull * -TwoWorlds.power
    self.shot.type = 'dynamic'
    self.shot.bullet = true
    self.shot.velocity = velocity
    self.shot.data.trail = {}
    self.shots[#self.shots + 1] = self.shot
    self.bodies[#self.bodies + 1] = self.shot
    if #self.shots > TwoWorlds.maxShots then
        self:remove(table.remove(self.shots, 1))
    end
    self.shot, self.dragging, self.reload = nil, false, 1
    self.launched = self.launched + 1
end

function TwoWorlds:pop(body)
    if body and body.valid and body.data.critter then
        self.poofs:fire(body.x, body.y)
        self.popped = self.popped + 1
        self:remove(body)
    end
end

-- Destroys a body and forgets it, wherever it is listed.
function TwoWorlds:remove(body)
    for _, list in ipairs({self.bodies, self.shots}) do
        for index = #list, 1, -1 do
            if list[index] == body then
                table.remove(list, index)
            end
        end
    end
    if body.valid then
        body:destroy()
    end
end

-- Returns the planet whose bubble holds a point, or nil in the open space between them.
function TwoWorlds:bubbleAt(x, y)
    for _, planet in ipairs({self.home, self.moon}) do
        local spec = planet.spec
        if (x - spec.x) ^ 2 + (y - spec.y) ^ 2 < spec.reach ^ 2 then
            return planet
        end
    end
    return nil
end

-- Moves every body that entered the bubble of the other planet to that world, and lets go of the ones lost in space.
function TwoWorlds:cross()
    for index = #self.bodies, 1, -1 do
        local body = self.bodies[index]
        local planet = self:bubbleAt(body.x, body.y)
        if planet and body.world ~= planet.world then
            local moved = body:moveToWorld(planet.world)
            self.bodies[index] = moved
            for position, shot in ipairs(self.shots) do
                if shot == body then
                    self.shots[position] = moved
                end
            end
            self.crossings = self.crossings + 1
            self:log('A body entered the bubble of %s at %.0f units per second.', planet.spec.name, moved.velocity:length())
        elseif body.x ^ 2 + body.y ^ 2 > TwoWorlds.lostDistance ^ 2 then
            self:remove(body)
        end
    end
end

-- Predicts the flight of a launch through both bubbles the way the worlds move it: a constant pull toward the planet whose bubble holds the point, the drag of its air, and a straight line between them, until it reaches a planet.
function TwoWorlds:predict(x, y, vx, vy)
    local points = self.path or {}
    local step = 1 / 60
    local count = 0
    for _ = 1, TwoWorlds.predictSteps do
        local planet = self:bubbleAt(x, y)
        if planet then
            local spec = planet.spec
            local dx, dy = spec.x - x, spec.y - y
            local distance = math.sqrt(dx * dx + dy * dy)
            if distance < spec.radius + TwoWorlds.shotRadius then
                break
            end
            vx, vy = vx + dx / distance * planet.pull * step, vy + dy / distance * planet.pull * step
            local keep = math.max(0, 1 - spec.drag * step)
            vx, vy = vx * keep, vy * keep
        end
        x, y = x + vx * step, y + vy * step
        count = count + 1
        points[count] = points[count] or {}
        points[count][1], points[count][2] = x, y
    end
    self.path, self.pathCount = points, count
end

function TwoWorlds:setPull(x, y)
    self.pull = m.vec2(x, y):clampedLength(TwoWorlds.maxPull)
end

function TwoWorlds:exit()
    TwoWorlds.super.exit(self)
    input.clearVirtual()
end

function TwoWorlds:update(dt)
    TwoWorlds.super.update(self, dt)
    if input.pressed('reset') then
        self:build()
    end
    local pointer = self.pointer
    local pouchX, pouchY = self:pouch()
    local dx, dy = pointer.worldX - pouchX, pointer.worldY - pouchY
    if pointer.pressed and self.shot and not ui.usingPointer() and dx * dx + dy * dy < TwoWorlds.maxPull * TwoWorlds.maxPull then
        self.dragging = true
    end
    if self.dragging then
        self:setPull(dx, dy)
        if pointer.released then
            self:launch()
        end
    end
    local aimX, aimY = input.vector('aim')
    if aimX ~= 0 or aimY ~= 0 then
        self:setPull(self.pull.x + aimX * 200 * dt, self.pull.y + aimY * 200 * dt)
    end
    if input.pressed('launch') then
        self:launch()
    end
    self.poofs:update(dt)

    local velocity = self.pull * -TwoWorlds.power
    self:predict(pouchX + self.pull.x, pouchY + self.pull.y, velocity.x, velocity.y)
    local onMoon = 0
    for _, body in ipairs(self.bodies) do
        onMoon = onMoon + (body.world == self.moon.world and 1 or 0)
    end
    self:status(string.format('%d shots launched and %d of %d critters popped. %d bodies are in the home world and %d in the moon world, after %d crossings.', self.launched, self.popped, self.critters, #self.bodies - onMoon, onMoon, self.crossings))
end

function TwoWorlds:fixedUpdate(step)
    if self.reload then
        self.reload = self.reload - step
        if self.reload <= 0 then
            self.reload = nil
            self:loadShot()
        end
    end
    if self.shot then
        local x, y = self:pouch()
        self.shot:setTransform(x + self.pull.x, y + self.pull.y, 0)
    end
    self:simulate(self.home.world, step)
    self:simulate(self.moon.world, step)
    self:cross()
    for _, shot in ipairs(self.shots) do
        local trail = shot.data.trail
        trail[#trail + 1] = shot.x
        trail[#trail + 1] = shot.y
        if #trail > TwoWorlds.trailLength * 2 then
            table.remove(trail, 1)
            table.remove(trail, 1)
        end
    end
end

-- A planet is its painted picture inside the soft bubble of its pull, with its name on a plate at the bottom of the bubble.
function TwoWorlds:drawPlanet(planet, picture)
    local spec = planet.spec
    local color = m.color(spec.rim)
    graphics2d.drawCircle(spec.x, spec.y, spec.reach, color:withAlpha(0.08), {layer = -20})
    graphics2d.drawRing(spec.x, spec.y, spec.reach, 3 * Look.pixel, color:withAlpha(0.45), {layer = -20})
    local size = spec.radius * 2 / TwoWorlds.cover
    graphics2d.draw(picture, spec.x, spec.y, {width = size, height = size, layer = 0})
    self:label(spec.name, spec.x, spec.y + spec.reach - 34, {size = 24})
end

-- A critter is a round green slime with eyes that look out from its planet.
function TwoWorlds:drawCritter(critter)
    local unit = Look.pixel
    local moon = self.moon.spec
    local x, y = critter.x, critter.y
    local dx, dy = x - moon.x, y - moon.y
    local length = math.max(math.sqrt(dx * dx + dy * dy), 1)
    local ux, uy = dx / length, dy / length
    graphics2d.drawShape({x - 20, y - 19, 40, 38}, {kind = 'ellipse', color = '#FF7BC74D', borderWidth = Look.outlinePixels * unit, borderColor = Look.ink, layer = 2})
    for _, side in ipairs({-1, 1}) do
        local ex, ey = x + ux * 5 - uy * side * 7, y + uy * 5 + ux * side * 7
        graphics2d.drawCircle(ex, ey, 5, '#FFFFFFFF', {layer = 2})
        graphics2d.drawCircle(ex + ux * 1.5, ey + uy * 1.5, 2.5, Look.ink, {layer = 2})
    end
end

-- The slingshot is a wooden fork on the top of the near planet, with its band around the shot.
function TwoWorlds:drawSlingshot()
    local pouchX, pouchY = self:pouch()
    local home = self.home.spec
    local wood = '#FFA86B3C'
    Look.line(pouchX, home.y - home.radius + 6, pouchX, pouchY + 14, 10, wood, 2)
    Look.line(pouchX - 26, pouchY - 6, pouchX, pouchY + 14, 8, wood, 2)
    Look.line(pouchX + 26, pouchY - 6, pouchX, pouchY + 14, 8, wood, 2)
    local band, stroke = self.band, self.bandStroke
    local bx, by = pouchX, pouchY - 6
    if self.shot then
        bx, by = self.shot.x, self.shot.y
    end
    band[1], band[2], band[3], band[4], band[5], band[6] = pouchX - 26, pouchY - 6, bx, by, pouchX + 26, pouchY - 6
    stroke.width = 5
    graphics2d.drawStroke(band, stroke)
end

function TwoWorlds:draw(area)
    Scenery.space()
    self:drawPlanet(self.home, self.pictures.home)
    self:drawPlanet(self.moon, self.pictures.moon)
    local stroke = self.trailStroke
    stroke.width = 3 * Look.pixel
    for _, shot in ipairs(self.shots) do
        if #shot.data.trail > 3 then
            graphics2d.drawStroke(shot.data.trail, stroke)
        end
    end
    for _, body in ipairs(self.bodies) do
        if body.valid and body.data.critter then
            self:drawCritter(body)
        end
    end
    Look.drawAll(self.bodies, {layer = 2})
    self:drawSlingshot()
    if self.shot then
        Look.draw(self.shot, {layer = 4})
        if self.showPath then
            local dots, count = self.dots, 0
            for index = 4, self.pathCount, 4 do
                dots[count * 2 + 1], dots[count * 2 + 2] = self.path[index][1], self.path[index][2]
                count = count + 1
            end
            if count > 0 then
                graphics2d.drawPoints(dots, {radius = 4 * Look.pixel, color = '#CCFFFFFF', count = count, layer = 4})
            end
        end
    end
    self.poofs:draw()
end

return TwoWorlds
