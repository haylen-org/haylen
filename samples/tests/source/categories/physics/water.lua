-- A tiled stone tank on a lawn whose water is a buoyancy force field: a boat and logs float, an iron anvil sinks, crates float deeper the denser they are, a current drags them along with the `flow` of the field, and waves come from a flow that changes over time.
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local input = require('haylen.input')
local m = require('haylen.math')
local physics2d = require('haylen.physics2d')
local ui = require('haylen.ui')

local Grab = require('categories.physics.grab')
local Look = require('categories.physics.look')
local PhysicsTest = require('categories.physics.physics-test')
local Scenery = require('categories.physics.scenery')

local Water = haylen.class('Water', PhysicsTest)

Water.code = [[
local pool = physics2d.newForceField(world, {kind = 'buoyancy', x = 0, y = 185, width = 1480, height = 410, density = 1, linearDrag = 1.5, angularDrag = 1.5})
crate:addBox(70, 70, {density = 0.4})  -- Floats, deeper the denser it is.
anvil:addPolygon(points, {density = 7})  -- Sinks.
pool.flow = {current + math.sin(time * 1.6) * 60, math.cos(time * 1.6) * 45}  -- A current with waves.]]

Water.surface, Water.bottom, Water.left, Water.right = -20, 390, -740, 740
Water.rim = -70
Water.tile = 46
Water.waveSpeed = 1.6
Water.maxBodies = 30
Water.dropY = -240
Water.anvilOutline = {{-62, -30}, {52, -30}, {52, -12}, {24, -4}, {32, 26}, {-32, 26}, {-24, -4}, {-44, -16}}
Water.hull = {{-95, -12}, {95, -12}, {70, 24}, {-70, 24}}
Water.cabin = Look.tint('steel', '#FFF2F3FF', '#FFC9CCE0', '#FFFFFFFF')
Water.bark = Look.tint('darkWood', '#FF8C5A32', '#FF5E3A20', '#FFB98052')

function Water:enter()
    self:frame{
        code = Water.code,
        hint = 'Drop things into the tank and drag them under: the lighter a crate, the higher it floats, and the anvil sinks. The current and the waves move the flow of the water. R or the X button refills the tank.',
        controls = {
            ui.button{id = 'crate', text = 'Drop a crate', onClick = function() self:dropCrate(self.random:range(-500, 500), Water.dropY, self.random:integer(1, 9) / 10) end},
            ui.button{id = 'log', text = 'Drop a log', onClick = function() self:dropLog(self.random:range(-500, 500), Water.dropY) end},
            ui.button{id = 'anvil', text = 'Drop an anvil', onClick = function() self:dropAnvil(self.random:range(-500, 500), Water.dropY) end},
            ui.label{text = 'Current', font = 'caption', color = 'textMuted'},
            ui.slider{id = 'current', value = 0, min = -200, max = 200, step = 10, showValue = true, decimals = 0, onChange = function(event) self.current = event.value end},
            ui.toggle{id = 'waves', text = 'Waves', checked = true, onChange = function(event) self.waves = event.checked end},
            ui.button{id = 'reset', text = 'Refill', onClick = function() self:build() end},
        },
        focus = 'crate',
        view = {1600, 700},
    }
    self.camera.position = {0, 75}
    self.random = m.random(51)
    self.current, self.waves, self.time = 0, true, 0
    self:build()
end

function Water:build()
    self.world = physics2d.newWorld()
    self.grab = Grab(self.world)
    self.bodies, self.crates, self.logs, self.anvils = {}, {}, {}, {}

    local tank = self.world:createBody({type = 'static'})
    tank:addBox(4000, 40, {offsetY = Water.bottom + 20})
    local height = Water.bottom - Water.rim
    tank:addBox(40, height, {offsetX = Water.left - 20, offsetY = Water.rim + height / 2})
    tank:addBox(40, height, {offsetX = Water.right + 20, offsetY = Water.rim + height / 2})
    self.water = physics2d.newForceField(self.world, {kind = 'buoyancy', x = 0, y = (Water.surface + Water.bottom) / 2, width = Water.right - Water.left, height = Water.bottom - Water.surface, density = 1, linearDrag = 1.5, angularDrag = 1.5})

    self.boat = self.world:createBody({x = -480, y = -60})
    self.boat:addPolygon(Water.hull, {density = 0.35})
    self.boat:addBox(70, 36, {offsetX = -10, offsetY = -30, density = 0.35})
    Look.dress(self.boat, 'red', {outline = Water.hull})
    self:add(self.boat)
    self:dropLog(-180, -80)
    self:dropLog(560, -120)
    self:dropAnvil(120, -160)
    for index = 1, 4 do
        self:dropCrate(-400 + index * 170, -120 - index * 30, index * 0.2)
    end
end

function Water:add(body)
    self.bodies[#self.bodies + 1] = body
    if #self.bodies > Water.maxBodies then
        table.remove(self.bodies, 1):destroy()
    end
    return body
end

-- A crate is the lighter and paler the less dense it is, and carries its density written above it.
function Water:dropCrate(x, y, density)
    local crate = self.world:createBody({x = x, y = y})
    crate:addBox(70, 70, {density = density})
    local light, dark = m.color('#FFF2C88E'), m.color('#FF8C5A32')
    local fill = light:lerp(dark, density)
    Look.dress(crate, Look.tint('crate', fill, fill:lerp(dark, 0.45), fill:lerp(light, 0.6)))
    crate.data.label = string.format('%.1f', density)
    self.crates[#self.crates + 1] = self:add(crate)
end

function Water:dropLog(x, y)
    local trunk = self.world:createBody({x = x, y = y, rotation = self.random:range(-0.2, 0.2)})
    trunk:addCapsule(-70, 0, 70, 0, 18, {density = 0.6})
    Look.dress(trunk, Water.bark)
    self.logs[#self.logs + 1] = self:add(trunk)
end

function Water:dropAnvil(x, y)
    local anvil = self.world:createBody({x = x, y = y})
    anvil:addPolygon(Water.anvilOutline, {density = 7})
    Look.dress(anvil, 'iron', {outline = Water.anvilOutline})
    self.anvils[#self.anvils + 1] = self:add(anvil)
end

function Water:update(dt)
    Water.super.update(self, dt)
    if input.pressed('reset') then
        self:build()
    end
    self.grab:update(self.pointer, self.camera)
    local resting = 0
    for _, anvil in ipairs(self.anvils) do
        resting = resting + ((anvil.valid and anvil.y > Water.bottom - 40) and 1 or 0)
    end
    local anvils = resting == 0 and 'no anvil has reached the bottom yet' or string.format('%d %s on the bottom', resting, resting == 1 and 'anvil lies' or 'anvils lie')
    local flow = self.water.flow
    self:status(string.format('%d bodies are in the water, %s, and the water flows at %d units per second.', self.water.bodyCount, anvils, math.floor(flow:length() + 0.5)))
end

-- Waves swing the flow around a circle, so floating bodies bob and rock, on top of the steady current.
function Water:fixedUpdate(step)
    self.time = self.time + step
    local x, y = self.current, 0
    if self.waves then
        x, y = x + math.sin(self.time * Water.waveSpeed) * 60, math.cos(self.time * Water.waveSpeed) * 45
    end
    self.water.flow = {x, y}
    self:simulate(self.world, step)
end

-- The boat is a red hull with a white cabin and a round window, which rocks with the body.
function Water:drawBoat()
    local boat = self.boat
    if not boat.valid then
        return
    end
    local cos, sin = math.cos(boat.rotation), math.sin(boat.rotation)
    local cx, cy = boat.x + (-10 * cos - -30 * sin), boat.y + (-10 * sin + -30 * cos)
    Look.block(cx, cy, 70, 36, Water.cabin, 1, boat.rotation)
    Look.disc(cx + 12 * cos, cy + 12 * sin, 8, 'glass', 1)
    Look.draw(boat, {layer = 1})
    graphics2d.drawLine(boat.x - 88 * cos + 2 * sin, boat.y - 88 * sin - 2 * cos, boat.x + 88 * cos + 2 * sin, boat.y + 88 * sin - 2 * cos, 5 * Look.pixel, '#FFFBF6EA', {layer = 1})
end

-- A log shows the pale rings of its cut end at its right end.
function Water:drawLog(log)
    if not log.valid then
        return
    end
    Look.draw(log, {layer = 1})
    local cos, sin = math.cos(log.rotation), math.sin(log.rotation)
    local x, y = log.x + cos * 76, log.y + sin * 76
    graphics2d.drawShape({x - 7, y - 17, 14, 34}, {kind = 'ellipse', rotation = log.rotation, color = '#FFE9C08A', borderWidth = 3 * Look.pixel, borderColor = Look.ink, layer = 1})
    graphics2d.drawShape({x - 3.5, y - 9, 7, 18}, {kind = 'ellipse', rotation = log.rotation, color = '#00000000', borderWidth = 1.5 * Look.pixel, borderColor = '#FFB8743A', layer = 1})
end

-- The back wall of the tank is pale tiles, which the water tints.
function Water:drawTiles()
    local unit = Look.pixel
    local top, size = Water.rim, Water.tile
    graphics2d.drawRect({Water.left, top, Water.right - Water.left, Water.bottom - top}, '#FFE4EEF5', {layer = -5})
    for y = top + size, Water.bottom - 1, size do
        graphics2d.drawLine(Water.left, y, Water.right, y, 2 * unit, '#FFB8CAD8', {layer = -5})
    end
    for x = Water.left + size, Water.right - 1, size do
        graphics2d.drawLine(x, top, x, Water.bottom, 2 * unit, '#FFB8CAD8', {layer = -5})
    end
end

function Water:draw(area)
    local bounds = graphics2d.canvasBounds()
    Scenery.sky(self.camera, Water.bottom)
    Scenery.ground(Water.bottom)
    self:drawTiles()
    Look.drawAll(self.crates, {layer = 1})
    Look.drawAll(self.anvils, {layer = 1})
    for _, log in ipairs(self.logs) do
        self:drawLog(log)
    end
    self:drawBoat()

    -- The water fills the tank between its walls and above its floor, and shows what floats and sinks in it.
    graphics2d.pushClip({Water.left, bounds.y, Water.right - Water.left, Water.bottom - bounds.y})
    Scenery.water(Water.left, Water.right, Water.surface, {layer = 3, alpha = 0.72})
    graphics2d.popClip()
    local height = Water.bottom - Water.rim
    for _, side in ipairs({-1, 1}) do
        Look.block(side * (Water.right + 20), Water.rim + height / 2, 40, height, 'stone', 4)
    end
    Look.block(0, Water.bottom + 10, Water.right - Water.left + 80, 20, 'stone', 4)
    for _, crate in ipairs(self.crates) do
        if crate.valid then
            self:tag(crate.data.label, crate.x, crate.y - 58, {size = 22})
        end
    end
    self.grab:draw()
end

return Water
