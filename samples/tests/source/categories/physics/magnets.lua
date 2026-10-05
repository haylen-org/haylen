-- Magnets as radial force fields from `physics2d.newForceField` whose mask picks the steel crates, so they pull or push the steel and leave the wood alone, switched on and off and dragged with the pointer.
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local input = require('haylen.input')
local m = require('haylen.math')
local physics2d = require('haylen.physics2d')
local ui = require('haylen.ui')

local parts = require('categories.physics.parts')
local PhysicsTest = require('categories.physics.physics-test')

local Magnets = haylen.class('Magnets', PhysicsTest)

local kSteel, kWood, kMagnet = 2, 4, 8
local kSteelColor, kWoodColor = '#FF90A4AE', '#FFBC8F5A'
local kRadius = 340
local kPoleOffset = 40
local kReach = 160
local kMaxSpeed = 1400
local kCrate = 54

function Magnets:enter()
    self:frame{
        hint = 'Drag a magnet over the crates: the red one pulls the steel crates, the blue one pushes them away, and the wooden crates feel neither. R or the X button rebuilds.',
        controls = {
            ui.checkbox{id = 'pull', text = 'Red magnet on', checked = true, onChange = function(event) self:switch(1, event.checked) end},
            ui.checkbox{id = 'push', text = 'Blue magnet on', checked = true, onChange = function(event) self:switch(2, event.checked) end},
            ui.label{text = 'Strength', font = 'caption', color = 'textMuted'},
            ui.slider{id = 'strength', value = 2600, min = 500, max = 5000, step = 100, showValue = true, decimals = 0, onChange = function(event) self:setStrength(event.value) end},
            ui.toggle{id = 'sweep', text = 'Sweep the magnets', onChange = function(event) self.sweeping = event.checked end},
            ui.button{id = 'reset', text = 'Rebuild', onClick = function() self:build() end},
        },
        focus = 'pull',
    }
    self.strength, self.enabled, self.sweeping, self.time = 2600, {true, true}, false, 0
    self:build()
end

function Magnets:build()
    self.world = physics2d.newWorld()
    self.crates, self.held = {}, nil

    local ground = self.world:createBody({type = 'static', x = 0, y = 410})
    parts.box(ground, 1600, 40)
    self.statics = {ground}
    local random = m.random(48)
    for pile = 0, 4 do
        for row = 0, 2 do
            local steel = random:chance(0.5)
            local crate = self.world:createBody({x = -560 + pile * 280 + random:range(-8, 8), y = 362 - row * (kCrate + 2)})
            parts.box(crate, kCrate, kCrate, {category = steel and kSteel or kWood, density = steel and 2 or 0.6})
            parts.paint(crate, steel and kSteelColor or kWoodColor)
            crate.data.steel = steel
            self.crates[#self.crates + 1] = crate
        end
    end

    self.magnets = {self:magnet(1, -300, 40, '#FFE57373'), self:magnet(2, 300, 40, '#FF64B5F6')}
end

-- A kinematic body holds the poles, so the crates the field pulls cling to it, and the field sits just under the poles. The first magnet pulls and the second one pushes.
function Magnets:magnet(index, x, y, color)
    local body = self.world:createBody({type = 'kinematic', x = x, y = y})
    body:addBox(100, 60, {category = kMagnet})
    local sign = index == 1 and 1 or -1
    local field = physics2d.newForceField(self.world, {kind = 'radial', x = x, y = y + kPoleOffset, radius = kRadius, strength = sign * self.strength, falloff = 'linear', linearDrag = 1.5, mask = kSteel, enabled = self.enabled[index]})
    return {body = body, field = field, sign = sign, color = color, home = {x, y}}
end

function Magnets:switch(index, enabled)
    self.enabled[index] = enabled
    self.magnets[index].field.enabled = enabled
end

function Magnets:setStrength(strength)
    self.strength = strength
    for _, magnet in ipairs(self.magnets) do
        magnet.field.strength = magnet.sign * strength
    end
end

function Magnets:update(dt)
    Magnets.super.update(self, dt)
    if input.pressed('reset') then
        self:build()
    end
    local pointer = self.pointer
    if pointer.pressed then
        for _, magnet in ipairs(self.magnets) do
            if math.sqrt((magnet.body.x - pointer.worldX) ^ 2 + (magnet.body.y - pointer.worldY) ^ 2) < kReach then
                self.held = magnet
            end
        end
    elseif not pointer.down then
        self.held = nil
    end
    if self.held then
        self.held.home = {m.clamp(pointer.worldX, -740, 740), m.clamp(pointer.worldY, -380, 300)}
    end
    local pull, push = self.magnets[1].field, self.magnets[2].field
    self:status(string.format('Steel crates pulled %d, pushed %d, strength %.0f, step %.2f ms', pull.bodyCount, push.bodyCount, self.strength, self:stepTime()))
end

-- Each magnet glides toward its home, or sweeps around it, at a limited speed, and its field follows only when it moves, so resting crates may sleep.
function Magnets:fixedUpdate(step)
    self.time = self.time + step
    for index, magnet in ipairs(self.magnets) do
        local x, y = magnet.home[1], magnet.home[2]
        if self.sweeping and self.held ~= magnet then
            x = m.clamp(x + math.sin(self.time * 0.8 + index * 2) * 300, -740, 740)
        end
        local body = magnet.body
        local dx, dy = x - body.x, y - body.y
        local distance = math.sqrt(dx * dx + dy * dy)
        if distance > 0.5 then
            local scale = math.min(1, kMaxSpeed * step / distance)
            local nextX, nextY = body.x + dx * scale, body.y + dy * scale
            body:moveTo(nextX, nextY, 0)
            magnet.field.position = {nextX, nextY + kPoleOffset}
        else
            body.velocity = {0, 0}
        end
    end
    self:simulate(self.world, step)
end

function Magnets:drawMagnet(magnet)
    local body, field = magnet.body, magnet.field
    local x, y = body.x, body.y
    local on = field.enabled
    local tint = on and magnet.color or '#FF5C6378'
    graphics2d.drawCircle(x, y + kPoleOffset, kRadius, m.color(tint):withAlpha(on and 0.07 or 0.03), {layer = -1})
    graphics2d.drawRing(x, y + kPoleOffset, kRadius, 2, m.color(tint):withAlpha(0.4), {layer = -1})
    graphics2d.drawArc(x, y + 10, 36, 24, math.pi, math.pi * 2, tint, {layer = 3})
    for _, side in ipairs({-1, 1}) do
        graphics2d.drawRect({x + side * 36 - 12, y + 10, 24, 20}, tint, {layer = 3})
        graphics2d.drawRect({x + side * 36 - 12, y + 30, 24, 12}, '#FFECEFF1', {layer = 3})
    end
    Magnets.caption(magnet.sign > 0 and 'Pull' or 'Push', x, y - 52, {anchor = {0.5, 0.5}, color = tint, layer = 4})
end

function Magnets:draw(area)
    parts.drawAll(self.statics)
    parts.drawAll(self.crates, {layer = 1})
    for _, crate in ipairs(self.crates) do
        if crate.data.steel then
            local cos, sin = math.cos(crate.rotation), math.sin(crate.rotation)
            for _, corner in ipairs({{-1, -1}, {1, -1}, {1, 1}, {-1, 1}}) do
                local lx, ly = corner[1] * 18, corner[2] * 18
                graphics2d.drawCircle(crate.x + lx * cos - ly * sin, crate.y + lx * sin + ly * cos, 4, '#FF546E7A', {layer = 2})
            end
        end
    end
    for _, magnet in ipairs(self.magnets) do
        self:drawMagnet(magnet)
    end
end

return Magnets
