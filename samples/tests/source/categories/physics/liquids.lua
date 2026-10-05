-- A particle fluid from `physics2d.newFluid` poured into a tank with floating crates and drawn as metaballs with `graphics2d.drawMetaballs`.
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local input = require('haylen.input')
local m = require('haylen.math')
local physics2d = require('haylen.physics2d')
local profiler = require('haylen.debug')
local ui = require('haylen.ui')

local Grab = require('categories.physics.grab')
local parts = require('categories.physics.parts')
local PhysicsTest = require('categories.physics.physics-test')

local Liquids = haylen.class('Liquids', PhysicsTest)

local kRadius = 6
local kMaxParticles = 1800
local kPourRate = 6
local kWater = 2

function Liquids:enter()
    self:frame{
        hint = 'Hold on the stage to pour water where you point, and drag the crates into it. R or the X button refills the tank.',
        controls = {
            ui.button{id = 'flood', text = 'Pour a wave', onClick = function() self.water:fill({-560, -380, 300, 160}, 200, 0) end},
            ui.checkbox{id = 'particles', text = 'Show the particles', onChange = function(event) self.showParticles = event.checked end},
            ui.label{text = 'Metaball threshold', font = 'caption', color = 'textMuted'},
            ui.slider{id = 'threshold', value = 0.5, min = 0.2, max = 0.8, step = 0.05, showValue = true, onChange = function(event) self.threshold = event.value end},
            ui.button{id = 'drain', text = 'Drain', onClick = function() self.water:clear() end},
            ui.button{id = 'reset', text = 'Refill', onClick = function() self:build() end},
        },
        focus = 'flood',
    }
    self.random = m.random(23)
    self.threshold = 0.5
    self.buffer = {}
    self:build()
end

function Liquids:build()
    self.world = physics2d.newWorld()
    self.grab = Grab(self.world, {mask = 1})
    self.crates = {}

    -- Thick walls push back the particles that the fluid pressure squeezes into them.
    local tank = self.world:createBody({type = 'static'})
    parts.box(tank, 40, 700, {offsetX = -660, offsetY = 50})
    parts.box(tank, 1360, 40, {offsetY = 400})
    parts.box(tank, 40, 700, {offsetX = 660, offsetY = 50})
    local shelf = self.world:createBody({type = 'static', x = 180, y = 120, rotation = -0.35})
    parts.box(shelf, 360, 20)
    local post = self.world:createBody({type = 'static', x = -200, y = 300})
    parts.box(post, 40, 160)
    self.statics = {tank, shelf, post}

    self.water = physics2d.newFluid(self.world, {radius = kRadius, smoothingRadius = 22, maxParticles = kMaxParticles, viscosity = 0.25, category = kWater})
    self.water:fill({-600, 60, 380, 300})
    for index = 0, 2 do
        local crate = self.world:createBody({x = 200 + index * 110, y = -200})
        parts.box(crate, 70, 50, {density = 0.4})
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
        for _ = 1, kPourRate do
            self.water:spawn(pointer.worldX + self.random:range(-12, 12), pointer.worldY + self.random:range(-12, 12), self.random:range(-40, 40), 120)
        end
    end
    local frame = profiler.frame()
    self:status(string.format('Particles %d of %d, fluid %.2f ms, physics step %.2f ms, frame %.1f ms', self.water.size, kMaxParticles, self:timing('fluid'), self:stepTime(), frame.milliseconds))
end

function Liquids:fixedUpdate(step)
    profiler.beginScope('fluid')
    self.water:update(step)
    profiler.endScope()
    self:simulate(self.world, step)
end

function Liquids:draw(area)
    parts.drawAll(self.statics)
    parts.drawAll(self.crates, {layer = 1})
    self.water:positions(self.buffer)
    if #self.buffer > 0 then
        graphics2d.drawMetaballs(self.buffer, kRadius * 2.4, {color = '#D04FA3F7', outlineColor = '#FFB3E5FC', outlineWidth = 0.08, threshold = self.threshold, layer = 2})
    end
    if self.showParticles then
        for index = 1, #self.buffer, 2 do
            graphics2d.drawCircle(self.buffer[index], self.buffer[index + 1], kRadius, '#FFFFFFFF', {layer = 3}, 8)
        end
    end
    self.grab:draw()
end

return Liquids
