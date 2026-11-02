-- Radial blasts from `physics2d.explode` with a falloff and optional occlusion, set off by a tap or a click on a tower of crates and on barrels behind a stone wall, with the impulse each body took drawn as an arrow.
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

local Explosions = haylen.class('Explosions', PhysicsTest)

Explosions.code = [[
local hits = physics2d.explode(world, {x = x, y = y, radius = 280, impulse = 900, falloff = 'quadratic', occlusion = true})
for _, hit in ipairs(hits) do
    arrow(hit.x, hit.y, hit.impulseX, hit.impulseY)  -- The impulse each body took.
end
camera:addTrauma(0.6)  -- Shakes the view.]]

Explosions.ground = 330
Explosions.radius = 280
Explosions.arrowLife = 2.4
-- The shock wave runs out to the radius of the blast in this many seconds, and the radius stays marked while the arrows show.
Explosions.waveTime = 0.35

function Explosions:enter()
    self:frame{
        code = Explosions.code,
        hint = 'Tap or click anywhere to set off a blast. The arrows show the impulse each body took. R or the X button rebuilds the tower.',
        controls = {
            ui.label{text = 'Falloff of the blast', font = 'caption', color = 'textMuted'},
            ui.radioGroup{id = 'falloff', items = {{id = 'none', text = 'None'}, {id = 'linear', text = 'Linear'}, {id = 'quadratic', text = 'Quadratic'}}, selected = 'linear', onChange = function(event) self.falloff = event.value end},
            ui.checkbox{id = 'occlusion', text = 'Walls shield what is behind them', onChange = function(event) self.occlusion = event.checked end},
            ui.label{text = 'Strength of the blast', font = 'caption', color = 'textMuted'},
            ui.slider{id = 'impulse', value = 900, min = 100, max = 2000, step = 50, showValue = true, decimals = 0, onChange = function(event) self.impulse = event.value end},
            ui.button{id = 'blast', text = 'Blast at random', onClick = function() self:explode(self.random:range(-500, 500), self.random:range(0, 300)) end},
            ui.button{id = 'reset', text = 'Rebuild', onClick = function() self:build() end},
        },
        focus = 'falloff',
    }
    self.random = m.random(13)
    self.falloff, self.occlusion, self.impulse = 'linear', false, 900
    self.arrows, self.arrowTime = {}, 0
    self.blast = Burst('explosions/explosion-large', {count = 3, scale = 0.7, layer = 30})
    self:build()
    timer.after(2.4, function() self:explode(-110, 300) end, {owner = self})
end

function Explosions:build()
    self.world = physics2d.newWorld({owner = self})
    self.bodies = {}
    local ground = self.world:createBody({type = 'static', x = 0, y = Explosions.ground + 30})
    ground:addBox(8000, 60)
    local wall = self.world:createBody({type = 'static', x = 380, y = Explosions.ground - 130})
    wall:addBox(64, 260)
    Look.dress(wall, 'stone')
    self.statics = {wall}

    for row = 0, 6 do
        for column = 0, 6 - row do
            local crate = self.world:createBody({x = -300 + column * 64 + row * 32, y = Explosions.ground - 31 - row * 62})
            crate:addBox(60, 60)
            Look.dress(crate, 'woodCrate')
            self.bodies[#self.bodies + 1] = crate
        end
    end
    for index = 0, 3 do
        local barrel = self.world:createBody({x = 480 + index * 72, y = Explosions.ground - 34})
        barrel:addBox(56, 66)
        Look.dress(barrel, 'barrel')
        self.bodies[#self.bodies + 1] = barrel
    end
end

function Explosions:explode(x, y)
    local hits = physics2d.explode(self.world, {x = x, y = y, radius = Explosions.radius, impulse = self.impulse, falloff = self.falloff, occlusion = self.occlusion})
    self.blast:fire(x, y, 0.6 + self.impulse / 2000)
    self.camera:addTrauma(0.35 + self.impulse / 4000)
    self.arrows, self.arrowTime = {}, 0
    self.center = {x = x, y = y}
    for _, hit in ipairs(hits) do
        self.arrows[#self.arrows + 1] = {x = hit.x, y = hit.y, dx = hit.impulseX / hit.body.mass, dy = hit.impulseY / hit.body.mass}
    end
    self.arrowTime = Explosions.arrowLife
    self.lastHits = #hits
end

function Explosions:update(dt)
    Explosions.super.update(self, dt)
    self.camera:update(dt)
    if input.pressed('reset') then
        self:build()
    end
    if self.pointer.pressed then
        self:explode(self.pointer.worldX, self.pointer.worldY)
    end
    self.blast:update(dt)
    self.arrowTime = math.max(0, self.arrowTime - dt)
    if self.lastHits then
        self:status(string.format('The last blast pushed %d bodies with a %s falloff%s.', self.lastHits, self.falloff == 'none' and 'flat' or self.falloff, self.occlusion and ', and the wall shielded what hides behind it' or ''))
    else
        self:status('Tap or click to set off a blast.')
    end
end

function Explosions:fixedUpdate(step)
    self:simulate(self.world, step)
end

-- The blast is a shock wave that runs out to its radius, and the radius stays marked with a dashed ring while the arrows show. Each arrow starts where the blast reached a body and points the way it pushed, as long as the speed it gave.
function Explosions:drawArrows()
    if self.arrowTime <= 0 then
        return
    end
    local unit = Look.pixel
    local age = Explosions.arrowLife - self.arrowTime
    local fade = math.min(1, self.arrowTime / 0.6)
    local center, radius = self.center, Explosions.radius
    if age < Explosions.waveTime then
        local reach = age / Explosions.waveTime
        graphics2d.drawRing(center.x, center.y, radius * reach, 18 * (1 - reach) + 4, {1, 0.93, 0.7, 0.8 * (1 - reach)}, {layer = 29, blend = 'additive'})
    end
    graphics2d.drawStroke(self:ring(center.x, center.y, radius), {width = 3 * unit, color = {1, 1, 1, 0.75 * fade}, closed = true, dashes = {12 * unit, 10 * unit}, layer = 39})
    local color = '#FFFFD93D'
    for _, arrow in ipairs(self.arrows) do
        local length = math.sqrt(arrow.dx * arrow.dx + arrow.dy * arrow.dy)
        if length > 60 then
            local scale = math.min(90, length * 0.08) / length
            local tx, ty = arrow.x + arrow.dx * scale, arrow.y + arrow.dy * scale
            local ux, uy = arrow.dx / length, arrow.dy / length
            Look.line(arrow.x, arrow.y, tx, ty, 3 * unit, color, 40)
            Look.line(tx, ty, tx - (ux * 10 - uy * 7) * unit, ty - (uy * 10 + ux * 7) * unit, 3 * unit, color, 40)
            Look.line(tx, ty, tx - (ux * 10 + uy * 7) * unit, ty - (uy * 10 - ux * 7) * unit, 3 * unit, color, 40)
        end
    end
end

-- Returns the points of a circle as a closed stroke, reusing one list.
function Explosions:ring(x, y, radius)
    local points = self.ringPoints or {}
    for index = 0, 63 do
        local angle = index / 64 * math.pi * 2
        points[index * 2 + 1], points[index * 2 + 2] = x + math.cos(angle) * radius, y + math.sin(angle) * radius
    end
    self.ringPoints = points
    return points
end

function Explosions:draw(area)
    Scenery.sky(self.camera, Explosions.ground)
    Scenery.ground(Explosions.ground)
    Look.drawAll(self.statics)
    Look.drawAll(self.bodies, {layer = 1})
    self:label('Barrels behind a wall', 588, Explosions.ground - 120, {size = 18})
    self.blast:draw()
    self:drawArrows()
end

return Explosions
