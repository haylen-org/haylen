-- Horseshoe magnets as radial force fields from `physics2d.newForceField` whose mask picks the steel crates, so they pull or push the steel and leave the wood alone, switched on and off and dragged with the pointer. The steel the red magnet pulls clings to its poles.
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local input = require('haylen.input')
local m = require('haylen.math')
local physics2d = require('haylen.physics2d')
local ui = require('haylen.ui')

local Look = require('categories.physics.look')
local PhysicsTest = require('categories.physics.physics-test')
local Scenery = require('categories.physics.scenery')

local Magnets = haylen.class('Magnets', PhysicsTest)

Magnets.code = [[
local magnet = physics2d.newForceField(world, {kind = 'radial', x = -300, y = 80, radius = 340, strength = 2600, falloff = 'linear', linearDrag = 1.5, mask = kSteel})
crate:addBox(54, 54, {category = kSteel, density = 2})  -- Wood takes another category and feels nothing.
magnet.strength = -2600  -- Pushes instead of pulling.
magnet.enabled = false
magnet.position = {x, y + 40}  -- Follows its kinematic body.]]

Magnets.steelCategory, Magnets.woodCategory, Magnets.magnetCategory = 2, 4, 8
Magnets.ground = 390
Magnets.radius = 340
-- The field sits this far under the center of a magnet, between its poles, whose ends are this far under it.
Magnets.poleOffset = 40
Magnets.poleEnd = 46
Magnets.reach = 160
Magnets.maxSpeed = 1400
Magnets.crate = 54
-- The field lines run in or out across the field this many times a second.
Magnets.flow = 0.45

function Magnets:enter()
    self:frame{
        code = Magnets.code,
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
    self.camera.position = {0, 60}
    self.strength, self.enabled, self.sweeping, self.time = 2600, {true, true}, false, 0
    self.horseshoe = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}
    self.horseshoeStroke = {width = 1, color = '#FFFFFFFF', join = 'round', cap = 'butt', layer = 3}
    self:build()
end

function Magnets:build()
    self.world = physics2d.newWorld()
    self.crates, self.held = {}, nil

    local ground = self.world:createBody({type = 'static', x = 0, y = Magnets.ground + 20})
    ground:addBox(4000, 40)
    local random = m.random(48)
    for pile = 0, 4 do
        for row = 0, 2 do
            local steel = random:chance(0.5)
            local crate = self.world:createBody({x = -560 + pile * 280 + random:range(-8, 8), y = Magnets.ground - Magnets.crate / 2 - row * (Magnets.crate + 2)})
            crate:addBox(Magnets.crate, Magnets.crate, {category = steel and Magnets.steelCategory or Magnets.woodCategory, density = steel and 2 or 0.6})
            Look.dress(crate, steel and 'steelCrate' or 'woodCrate')
            self.crates[#self.crates + 1] = crate
        end
    end

    self.magnets = {self:magnet(1, -300, 40, Look.toys[1]), self:magnet(2, 300, 40, Look.toys[2])}
end

-- A kinematic body as large as the horseshoe holds it, so the crates the field pulls cling to the ends of its poles, and the field sits just above them. The first magnet pulls and the second one pushes.
function Magnets:magnet(index, x, y, material)
    local body = self.world:createBody({type = 'kinematic', x = x, y = y})
    body:addBox(100, 30 + Magnets.poleEnd, {offsetY = (Magnets.poleEnd - 30) / 2, category = Magnets.magnetCategory})
    local sign = index == 1 and 1 or -1
    local field = physics2d.newForceField(self.world, {kind = 'radial', x = x, y = y + Magnets.poleOffset, radius = Magnets.radius, strength = sign * self.strength, falloff = 'linear', linearDrag = 1.5, mask = Magnets.steelCategory, enabled = self.enabled[index]})
    return {body = body, field = field, sign = sign, material = material, home = {x, y}}
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
            if math.sqrt((magnet.body.x - pointer.worldX) ^ 2 + (magnet.body.y - pointer.worldY) ^ 2) < Magnets.reach then
                self.held = magnet
            end
        end
    elseif not pointer.down then
        self.held = nil
    end
    if self.held then
        self.held.home = {m.clamp(pointer.worldX, -740, 740), m.clamp(pointer.worldY, -300, Magnets.ground - 110)}
    end
    local pull, push = self.magnets[1].field, self.magnets[2].field
    local pulled = pull.enabled and string.format('The red magnet pulls %d steel %s', pull.bodyCount, pull.bodyCount == 1 and 'crate' or 'crates') or 'The red magnet is off'
    local pushed = push.enabled and string.format('the blue one pushes %d', push.bodyCount) or 'the blue one is off'
    self:status(string.format('%s, %s, and the wooden crates feel neither.', pulled, pushed))
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
            local scale = math.min(1, Magnets.maxSpeed * step / distance)
            local nextX, nextY = body.x + dx * scale, body.y + dy * scale
            body:moveTo(nextX, nextY, 0)
            magnet.field.position = {nextX, nextY + Magnets.poleOffset}
        else
            body.velocity = {0, 0}
        end
    end
    self:simulate(self.world, step)
end

-- The field shows its reach as a soft disc with three rings that flow toward the poles of a magnet that pulls and away from one that pushes.
function Magnets:drawField(magnet)
    local field = magnet.field
    if not field.enabled then
        return
    end
    local x, y = magnet.body.x, magnet.body.y + Magnets.poleOffset
    local color = magnet.material.fill
    local tint = m.color(color)
    graphics2d.drawCircle(x, y, Magnets.radius, tint:withAlpha(0.08), {layer = -5})
    for ring = 0, 2 do
        local phase = (self.time * Magnets.flow + ring / 3) % 1
        local share = magnet.sign > 0 and 1 - phase or phase
        local alpha = math.sin(share * math.pi) * 0.55
        graphics2d.drawRing(x, y, 50 + share * (Magnets.radius - 50), 4 * Look.pixel, tint:withAlpha(alpha), {layer = -5})
    end
end

-- The horseshoe is one thick stroke in the color of the magnet, so its arch and its legs meet without a seam, with steel pole ends and a highlight along its arch.
function Magnets:drawMagnet(magnet)
    local unit = Look.pixel
    local body = magnet.body
    local x, y = body.x, body.y
    local material = magnet.field.enabled and magnet.material or Look.materials.slate
    local path, stroke = self.horseshoe, self.horseshoeStroke
    local legs, top, bottom = 36, y, y + 30
    path[1], path[2] = x - legs, bottom
    for step = 0, 8 do
        local angle = math.pi + math.pi * step / 8
        path[3 + step * 2], path[4 + step * 2] = x + math.cos(angle) * legs, top + math.sin(angle) * legs
    end
    path[21], path[22] = x + legs, bottom
    stroke.width, stroke.color = 28 + Look.outlinePixels * unit * 2, Look.ink
    graphics2d.drawStroke(path, stroke)
    stroke.width, stroke.color = 28, material.fill
    graphics2d.drawStroke(path, stroke)
    graphics2d.drawArc(x - 2, top - 2, legs + 6, 5, math.pi * 1.15, math.pi * 1.6, material.light, {layer = 3})
    for _, side in ipairs({-1, 1}) do
        Look.block(x + side * legs, bottom + (Magnets.poleEnd - 30) / 2, 28, Magnets.poleEnd - 30 + 4, 'steel', 3)
    end
    self:label(magnet.sign > 0 and 'Pulls steel' or 'Pushes steel', x, y - 74, {size = 22, plate = material.shade})
end

function Magnets:draw(area)
    Scenery.sky(self.camera, Magnets.ground)
    Scenery.ground(Magnets.ground)
    for _, magnet in ipairs(self.magnets) do
        self:drawField(magnet)
    end
    Look.drawAll(self.crates, {layer = 1})
    for _, magnet in ipairs(self.magnets) do
        self:drawMagnet(magnet)
    end
end

return Magnets
