-- A pool that is a buoyancy force field: boats and logs float, an anvil sinks, crates float deeper the denser they are, a current drags them along with the `flow` of the field, and waves come from a flow that changes over time.
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local input = require('haylen.input')
local m = require('haylen.math')
local physics2d = require('haylen.physics2d')
local ui = require('haylen.ui')

local Grab = require('categories.physics.grab')
local parts = require('categories.physics.parts')
local PhysicsTest = require('categories.physics.physics-test')

local Water = haylen.class('Water', PhysicsTest)

local kSurface, kBottom, kLeft, kRight = -20, 390, -740, 740
local kWaveSpeed = 1.6
local kMaxBodies = 30
local kAnvil = {{-62, -30}, {52, -30}, {52, -12}, {24, -4}, {32, 26}, {-32, 26}, {-24, -4}, {-44, -16}}
local kHull = {{-95, -12}, {95, -12}, {70, 24}, {-70, 24}}

function Water:enter()
    self:frame{
        hint = 'Drop things into the pool and drag them under: the lighter a crate, the higher it floats, and the anvil sinks. The current and the waves move the flow of the water. R or the X button refills the pool.',
        controls = {
            ui.button{id = 'crate', text = 'Drop a crate', onClick = function() self:dropCrate(self.random:range(-500, 500), -300, self.random:integer(1, 9) / 10) end},
            ui.button{id = 'log', text = 'Drop a log', onClick = function() self:dropLog(self.random:range(-500, 500), -300) end},
            ui.button{id = 'anvil', text = 'Drop an anvil', onClick = function() self:dropAnvil(self.random:range(-500, 500), -300) end},
            ui.label{text = 'Current', font = 'caption', color = 'textMuted'},
            ui.slider{id = 'current', value = 0, min = -200, max = 200, step = 10, showValue = true, decimals = 0, onChange = function(event) self.current = event.value end},
            ui.toggle{id = 'waves', text = 'Waves', checked = true, onChange = function(event) self.waves = event.checked end},
            ui.button{id = 'reset', text = 'Refill', onClick = function() self:build() end},
        },
        focus = 'crate',
    }
    self.random = m.random(51)
    self.current, self.waves, self.time = 0, true, 0
    self:build()
end

function Water:build()
    self.world = physics2d.newWorld()
    self.grab = Grab(self.world)
    self.bodies = {}

    local pool = self.world:createBody({type = 'static'})
    parts.box(pool, 1600, 40, {offsetY = 410})
    parts.box(pool, 40, 560, {offsetX = kLeft - 20, offsetY = 130})
    parts.box(pool, 40, 560, {offsetX = kRight + 20, offsetY = 130})
    self.statics = {pool}
    self.water = physics2d.newForceField(self.world, {kind = 'buoyancy', x = 0, y = (kSurface + kBottom) / 2, width = kRight - kLeft, height = kBottom - kSurface, density = 1, linearDrag = 1.5, angularDrag = 1.5})

    local boat = self:add(self.world:createBody({x = -480, y = -60}), '#FFEF5350')
    parts.polygon(boat, kHull, {density = 0.35})
    parts.box(boat, 70, 36, {offsetX = -10, offsetY = -30, density = 0.35})
    self:dropLog(-180, -80)
    self:dropLog(560, -120)
    self:dropAnvil(120, -200)
    for index = 1, 4 do
        self:dropCrate(-400 + index * 170, -250 - index * 40, index * 0.2)
    end
end

function Water:add(body, color)
    self.bodies[#self.bodies + 1] = body
    if #self.bodies > kMaxBodies then
        table.remove(self.bodies, 1):destroy()
    end
    parts.paint(body, color)
    return body
end

function Water:dropCrate(x, y, density)
    local crate = self.world:createBody({x = x, y = y})
    parts.box(crate, 70, 70, {density = density})
    self:add(crate, m.color(1, 0.85, 0.4):lerp(m.color(0.55, 0.35, 0.2), density))
    crate.data.label = string.format('%.1f', density)
end

function Water:dropLog(x, y)
    local trunk = self.world:createBody({x = x, y = y, rotation = self.random:range(-0.2, 0.2)})
    parts.capsule(trunk, -70, 0, 70, 0, 18, {density = 0.6})
    self:add(trunk, '#FFA1887F')
end

function Water:dropAnvil(x, y)
    local anvil = self.world:createBody({x = x, y = y})
    parts.polygon(anvil, kAnvil, {density = 7})
    self:add(anvil, '#FF546E7A')
end

function Water:update(dt)
    Water.super.update(self, dt)
    if input.pressed('reset') then
        self:build()
    end
    self.grab:update(self.pointer, self.camera)
    local flow = self.water.flow
    self:status(string.format('Bodies in the water %d, flow %.0f, %.0f, step %.2f ms', self.water.bodyCount, flow.x, flow.y, self:stepTime()))
end

-- Waves swing the flow around a circle, so floating bodies bob and rock, on top of the steady current.
function Water:fixedUpdate(step)
    self.time = self.time + step
    local x, y = self.current, 0
    if self.waves then
        x, y = x + math.sin(self.time * kWaveSpeed) * 60, math.cos(self.time * kWaveSpeed) * 45
    end
    self.water.flow = {x, y}
    self:simulate(self.world, step)
end

function Water:draw(area)
    parts.drawAll(self.statics)
    parts.drawAll(self.bodies, {layer = 1})
    for _, body in ipairs(self.bodies) do
        if body.valid and body.data.label then
            graphics2d.drawText(nil, body.data.label, body.x, body.y, {size = 24, color = '#FF3E2723', anchor = {0.5, 0.5}, rotation = body.rotation, layer = 2})
        end
    end

    -- The surface ripples with the phase of the waves, over a flat surface that the field floats bodies on.
    local surface = {}
    local amplitude = self.waves and 6 or 0
    for x = kLeft, kRight, 40 do
        surface[#surface + 1] = {x, kSurface + math.sin(x / 90 - self.time * kWaveSpeed) * amplitude}
    end
    local fill = {table.unpack(surface)}
    fill[#fill + 1] = {kRight, kBottom}
    fill[#fill + 1] = {kLeft, kBottom}
    graphics2d.drawPolygon(fill, '#7029B6F6', {layer = 3})
    graphics2d.drawPolyline(surface, 4, '#CCB3E5FC', false, {layer = 3})
    self.grab:draw()
end

return Water
