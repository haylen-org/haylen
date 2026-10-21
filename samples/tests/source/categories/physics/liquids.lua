-- A particle fluid from `physics2d.newFluid` poured into a stone tank where light crates float and a heavy one sinks, stepped in C++ inside the world step and drawn as metaballs straight from its arrays with `fluid:draw`, between the crates and the walls of the tank.
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

local Liquids = haylen.class('Liquids', PhysicsTest)

Liquids.code = [[
local water = physics2d.newFluid(world, {radius = 6, smoothingRadius = 22, maxParticles = 4000, viscosity = 0.25})
water:fill({-620, -20, 380, 320})
water:spawn(x, y, 0, 120)  -- Pours where the pointer holds.
crate:addBox(70, 50, {density = 0.4})  -- Lighter than the water, so it floats.
water:draw({radius = 10, color = '#D838B8E0', outlineColor = '#FFD8F6FF', outlineWidth = 0.06, layer = 4})]]

Liquids.radius = 6
Liquids.drawRadius = 10
Liquids.maxParticles = 4000
Liquids.pourRate = 8
Liquids.ground = 340

function Liquids:enter()
    self:frame{
        code = Liquids.code,
        hint = 'Hold on the stage to pour water where you point, and drag the crates into it: the light ones float and the heavy one sinks. R or the X button refills the tank.',
        controls = {
            ui.button{id = 'flood', text = 'Pour a wave', onClick = function() self.water:fill({-560, -380, 300, 160}, 200, 0) end},
            ui.checkbox{id = 'particles', text = 'Show the particles', onChange = function(event) self.showParticles = event.checked end},
            ui.label{text = 'Threshold of the surface', font = 'caption', color = 'textMuted'},
            ui.slider{id = 'threshold', value = 0.5, min = 0.2, max = 0.8, step = 0.05, showValue = true, onChange = function(event) self.threshold = event.value end},
            ui.button{id = 'drain', text = 'Drain', onClick = function() self.water:clear() end},
            ui.button{id = 'reset', text = 'Refill', onClick = function() self:build() end},
        },
        focus = 'flood',
    }
    self.random = m.random(23)
    self.threshold = 0.5
    self.drawOptions = {radius = Liquids.drawRadius, color = '#D838B8E0', outlineColor = '#FFD8F6FF', outlineWidth = 0.06, threshold = 0.5, layer = 4}
    self:build()
end

function Liquids:build()
    self.world = physics2d.newWorld()
    self.grab = Grab(self.world)
    self.crates = {}

    -- Thick walls push back the particles that the fluid pressure squeezes into them.
    local tank = self.world:createBody({type = 'static'})
    tank:addBox(40, 700, {offsetX = -660, offsetY = -10})
    tank:addBox(1360, 40, {offsetY = 320})
    tank:addBox(40, 700, {offsetX = 660, offsetY = -10})
    Look.dress(tank, 'stone')
    local shelf = self.world:createBody({type = 'static', x = 180, y = 60, rotation = -0.35})
    shelf:addBox(360, 20)
    Look.dress(shelf, 'wood')
    local post = self.world:createBody({type = 'static', x = -200, y = 230})
    post:addBox(40, 140)
    Look.dress(post, 'stone')
    self.statics = {tank, shelf, post}

    self.water = physics2d.newFluid(self.world, {radius = Liquids.radius, smoothingRadius = 22, maxParticles = Liquids.maxParticles, viscosity = 0.25})
    self.water:fill({-620, -20, 380, 320})
    self.water:fill({-160, 140, 790, 160})
    for index = 0, 3 do
        local heavy = index == 3
        local crate = self.world:createBody({x = -300 + index * 200, y = heavy and 80 or -290})
        crate:addBox(70, 50, {density = heavy and 4 or 0.4})
        Look.dress(crate, heavy and 'iron' or 'wood')
        self.crates[#self.crates + 1] = crate
    end
end

function Liquids:update(dt)
    Liquids.super.update(self, dt)
    if input.pressed('reset') then
        self:build()
    end
    local pointer = self.pointer
    self.grab:update(pointer, self.camera)
    if pointer.down and not self.grab:holding() then
        for _ = 1, Liquids.pourRate do
            self.water:spawn(pointer.worldX + self.random:range(-12, 12), pointer.worldY + self.random:range(-12, 12), self.random:range(-40, 40), 120)
        end
    end
    self:status(string.format('%d drops of water of %d, simulated in %.2f ms of a physics step of %.2f ms.', self.water.size, Liquids.maxParticles, self.water.stepMilliseconds, self:stepTime()))
end

function Liquids:fixedUpdate(step)
    self:simulate(self.world, step)
end

-- The back of the tank is laid with tiles, so the water shows against them.
function Liquids:drawTiles()
    local unit = Look.pixel
    graphics2d.drawRect({-640, -360, 1280, 660}, '#FF8CD2E2', {layer = 0})
    for x = -640, 640, 80 do
        graphics2d.drawLine(x, -360, x, 300, 2 * unit, '#FF6AB4C8', {layer = 0})
    end
    for y = -360, 300, 80 do
        graphics2d.drawLine(-640, y, 640, y, 2 * unit, '#FF6AB4C8', {layer = 0})
    end
end

function Liquids:draw(area)
    Scenery.sky(self.camera, Liquids.ground)
    Scenery.ground(Liquids.ground)
    self:drawTiles()
    Look.drawAll(self.crates, {layer = 2})
    self.drawOptions.threshold = self.threshold
    self.water:draw(self.drawOptions)
    Look.drawAll(self.statics, {layer = 6})
    if self.showParticles then
        self.buffer = self.water:positions(self.buffer)
        for index = 1, #self.buffer, 2 do
            graphics2d.drawCircle(self.buffer[index], self.buffer[index + 1], Liquids.radius, '#FFFFFFFF', {layer = 7})
        end
    end
    self.grab:draw()
end

return Liquids
