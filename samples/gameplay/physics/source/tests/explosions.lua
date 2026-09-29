-- Radial blasts from physics2d.explode with a falloff and optional occlusion, set off by a tap or a click on a tower of crates and barrels.
local haylen = require('haylen')
local graphics2d = require('haylen.graphics2d')
local input = require('haylen.input')
local m = require('haylen.math')
local physics2d = require('haylen.physics2d')
local ui = require('haylen.ui')

local parts = require('parts')
local sample = require('sample')

local Explosions = haylen.class('Explosions', sample.Test)

local kRadius = 280
local kBlastLife = 0.5

function Explosions:enter()
    Explosions.super.enter(self, {
        hint = 'Tap or click anywhere to set off a blast. The arrows show the impulse each body took. R or X rebuilds the tower.',
        controls = {
            ui.label{text = 'Falloff', font = 'caption', color = 'textMuted'},
            ui.radioGroup{id = 'falloff', items = {{id = 'none', text = 'None'}, {id = 'linear', text = 'Linear'}, {id = 'quadratic', text = 'Quadratic'}}, selected = 'linear', onChange = function(event) self.falloff = event.value end},
            ui.checkbox{id = 'occlusion', text = 'Occlusion behind walls', onChange = function(event) self.occlusion = event.checked end},
            ui.label{text = 'Impulse', font = 'caption', color = 'textMuted'},
            ui.slider{id = 'impulse', value = 900, min = 100, max = 2000, step = 50, showValue = true, decimals = 0, onChange = function(event) self.impulse = event.value end},
            ui.button{id = 'blast', text = 'Blast at random', onClick = function() self:explode(self.random:range(-500, 500), self.random:range(0, 350)) end},
            ui.button{id = 'reset', text = 'Rebuild', onClick = function() self:build() end},
        },
        stats = true,
        focus = 'falloff',
    })
    self.random = m.random(13)
    self.falloff, self.occlusion, self.impulse = 'linear', false, 900
    self.blasts = {}
    self:build()
end

function Explosions:build()
    self.world = physics2d.newWorld()
    self.bodies = {}
    local ground = self.world:createBody({type = 'static', x = 0, y = 410})
    parts.box(ground, 1600, 40)
    local wall = self.world:createBody({type = 'static', x = 380, y = 270})
    parts.box(wall, 40, 240)
    self.statics = {ground, wall}

    for row = 0, 6 do
        for column = 0, 6 - row do
            local crate = self.world:createBody({x = -300 + column * 64 + row * 32, y = 358 - row * 62})
            parts.box(crate, 60, 60)
            self.bodies[#self.bodies + 1] = crate
        end
    end
    for index = 0, 3 do
        local barrel = self.world:createBody({x = 480 + index * 70, y = 355})
        parts.circle(barrel, 34)
        parts.paint(barrel, '#FFFF8A65')
        self.bodies[#self.bodies + 1] = barrel
    end
end

function Explosions:explode(x, y)
    local hits = physics2d.explode(self.world, {x = x, y = y, radius = kRadius, impulse = self.impulse, falloff = self.falloff, occlusion = self.occlusion})
    self.blasts[#self.blasts + 1] = {x = x, y = y, hits = hits, life = kBlastLife}
    self.lastHits = #hits
end

function Explosions:exit()
    Explosions.super.exit(self)
    self.world, self.bodies, self.statics, self.blasts = nil, nil, nil, nil
end

function Explosions:update(dt)
    Explosions.super.update(self, dt)
    if input.pressed('reset') then
        self:build()
    end
    if self.pointer.pressed then
        self:explode(self.pointer.worldX, self.pointer.worldY)
    end
    for index = #self.blasts, 1, -1 do
        local blast = self.blasts[index]
        blast.life = blast.life - dt
        if blast.life <= 0 then
            table.remove(self.blasts, index)
        end
    end
    self:showStats(string.format('bodies hit %d\nfalloff %s\nstep %.2f ms', self.lastHits or 0, self.falloff, sample.milliseconds('physics step')))
end

function Explosions:fixedUpdate(step)
    sample.step(self.world, step)
end

function Explosions:render()
    self:beginWorld()
    parts.drawAll(self.statics)
    parts.drawAll(self.bodies)
    for _, blast in ipairs(self.blasts) do
        local age = 1 - blast.life / kBlastLife
        graphics2d.drawCircle(blast.x, blast.y, kRadius * age, m.color(1, 0.6, 0.2, 0.35 * (1 - age)), {layer = 5, blend = 'additive'})
        graphics2d.drawRing(blast.x, blast.y, kRadius, 2, m.color(1, 0.8, 0.4, 1 - age), {layer = 5})
        for _, hit in ipairs(blast.hits) do
            local length = math.sqrt(hit.impulseX ^ 2 + hit.impulseY ^ 2)
            local scale = math.min(120, length / hit.body.mass * 0.12) / math.max(length, 0.001)
            graphics2d.drawLine(hit.x, hit.y, hit.x + hit.impulseX * scale, hit.y + hit.impulseY * scale, 4, m.color(1, 1, 0.5, 1 - age), {layer = 6})
        end
    end
end

return Explosions
