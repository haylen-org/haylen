-- Fans as directional force fields that push every body inside them with the same force, so light leaves fly and heavy crates stay, gusts that change their strength over time and a vortex field that swirls what the updraft lifts.
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local input = require('haylen.input')
local m = require('haylen.math')
local physics2d = require('haylen.physics2d')
local ui = require('haylen.ui')

local Grab = require('categories.physics.grab')
local parts = require('categories.physics.parts')
local PhysicsTest = require('categories.physics.physics-test')

local Wind = haylen.class('Wind', PhysicsTest)

-- Each fan blows through a rectangle from its housing, given by the center, size and direction of the rectangle and the place of the housing.
local kFans = {
    {x = -210, y = 320, width = 1100, height = 140, direction = {1, 0}, hub = {-772, 320}, group = 'floor'},
    {x = 660, y = 320, width = 200, height = 140, direction = {-1, 0}, hub = {772, 320}, group = 'floor'},
    {x = 450, y = 70, width = 220, height = 640, direction = {0, -1}, hub = {450, 378}, group = 'updraft'},
}
local kVortex = {450, -230, 170}
local kLeafColors = {'#FF9CCC65', '#FFFFB74D', '#FFE57373', '#FFC5E1A5'}
local kMaxLeaves = 40

function Wind:enter()
    self:frame{
        hint = 'Fans push every body with the same force: leaves fly, balloons drift, the cardboard box slides in the gusts and the crate never moves. Drag anything around. R or the X button rebuilds.',
        controls = {
            ui.checkbox{id = 'floor', text = 'Floor fans', checked = true, onChange = function(event) self:switch('floor', event.checked) end},
            ui.checkbox{id = 'updraft', text = 'Updraft fan', checked = true, onChange = function(event) self:switch('updraft', event.checked) end},
            ui.checkbox{id = 'vortex', text = 'Vortex', checked = true, onChange = function(event) self:switch('vortex', event.checked) end},
            ui.toggle{id = 'gusts', text = 'Gusts', checked = true, onChange = function(event) self.gusts = event.checked end},
            ui.label{text = 'Fan force', font = 'caption', color = 'textMuted'},
            ui.slider{id = 'force', value = 14, min = 0, max = 60, step = 1, showValue = true, decimals = 0, onChange = function(event) self.force = event.value end},
            ui.button{id = 'leaves', text = 'Drop leaves', onClick = function() self:dropLeaves(6) end},
            ui.button{id = 'reset', text = 'Rebuild', onClick = function() self:build() end},
        },
        focus = 'floor',
    }
    self.force, self.gusts, self.time = 14, true, 0
    self.enabled = {floor = true, updraft = true, vortex = true}
    self.noise = m.noise(50)
    self.random = m.random(50)
    self:build()
end

function Wind:build()
    self.world = physics2d.newWorld()
    self.grab = Grab(self.world)
    self.bodies, self.leaves = {}, {}

    local room = self.world:createBody({type = 'static'})
    parts.box(room, 1600, 40, {offsetY = 410})
    parts.box(room, 1600, 20, {offsetY = -430})
    parts.box(room, 20, 860, {offsetX = -790})
    parts.box(room, 20, 860, {offsetX = 790})
    self.statics = {room}

    self.fans = {}
    for _, spec in ipairs(kFans) do
        local field = physics2d.newForceField(self.world, {kind = 'directional', x = spec.x, y = spec.y, width = spec.width, height = spec.height, direction = spec.direction, strength = self.force, acceleration = false, enabled = self.enabled[spec.group]})
        self.fans[#self.fans + 1] = {field = field, spec = spec, spin = 0}
    end
    self.vortex = physics2d.newForceField(self.world, {kind = 'vortex', x = kVortex[1], y = kVortex[2], radius = kVortex[3], strength = self.force / 2, acceleration = false, enabled = self.enabled.vortex})

    self.bodies[1] = self:body({x = -600, y = 340}, '#FF8D6E63', function(body) parts.box(body, 64, 64) end)
    self.bodies[2] = self:body({x = -420, y = 340}, '#FFD7B98E', function(body) parts.box(body, 60, 60, {density = 0.05}) end)
    self.bodies[3] = self:body({x = -250, y = 300}, '#FF4DD0E1', function(body) parts.circle(body, 30, {density = 0.08, restitution = 0.4}) end)
    for index = 1, 4 do
        local balloon = self:body({x = -500 + index * 150, y = -200, gravityScale = 0.1, linearDamping = 0.8}, kLeafColors[index], function(body) parts.circle(body, 26, {density = 0.05}) end)
        balloon.data.balloon = true
        self.bodies[#self.bodies + 1] = balloon
    end
    self:dropLeaves(14)
end

function Wind:body(options, color, build)
    local body = self.world:createBody(options)
    build(body)
    parts.paint(body, color)
    return body
end

function Wind:dropLeaves(count)
    for _ = 1, count do
        local options = {x = self.random:range(-700, 300), y = self.random:range(-350, 100), rotation = self.random:range(0, math.pi), linearDamping = 3, angularDamping = 1.5}
        self.leaves[#self.leaves + 1] = self:body(options, kLeafColors[self.random:integer(1, #kLeafColors)], function(body) parts.box(body, 34, 10, {density = 0.1}) end)
        if #self.leaves > kMaxLeaves then
            table.remove(self.leaves, 1):destroy()
        end
    end
end

function Wind:switch(group, enabled)
    self.enabled[group] = enabled
    if group == 'vortex' then
        self.vortex.enabled = enabled
        return
    end
    for _, fan in ipairs(self.fans) do
        if fan.spec.group == group then
            fan.field.enabled = enabled
        end
    end
end

function Wind:update(dt)
    Wind.super.update(self, dt)
    if input.pressed('reset') then
        self:build()
    end
    self.grab:update(self.pointer, self.camera)
    local floor, updraft = self.fans[1].field, self.fans[3].field
    self:status(string.format('Floor fan force %.1f pushing %d bodies, updraft %.1f pushing %d, vortex %d, step %.2f ms', floor.strength, floor.bodyCount, updraft.strength, updraft.bodyCount, self.vortex.bodyCount, self:stepTime()))
end

-- Gusts move the strength of every fan along its own line of noise.
function Wind:fixedUpdate(step)
    self.time = self.time + step
    for index, fan in ipairs(self.fans) do
        local gust = self.gusts and 1 + 0.9 * self.noise:perlin(self.time * 0.6, index * 7) or 1
        fan.field.strength = self.force * gust
        fan.spin = fan.spin + (fan.field.enabled and step * fan.field.strength * 0.02 or 0)
    end
    self.vortex.strength = self.force / 2
    self:simulate(self.world, step)
end

-- Streaks slide along the direction of a fan at a speed that follows its strength.
function Wind:drawFan(fan)
    local spec, field = fan.spec, fan.field
    local dx, dy = spec.direction[1], spec.direction[2]
    local length = math.abs(dx) > 0 and spec.width or spec.height
    local across = math.abs(dx) > 0 and spec.height or spec.width
    if field.enabled then
        local alpha = math.min(0.5, field.strength / 40)
        for lane = 1, 5 do
            local side = (lane / 6 - 0.5) * across
            local along = (self.time * (200 + field.strength * 12) + lane * 137) % length - length / 2
            local x, y = spec.x + dx * along - dy * side, spec.y + dy * along + dx * side
            graphics2d.drawLine(x, y, x + dx * 60, y + dy * 60, 3, m.color(1, 1, 1, alpha), {layer = -1})
        end
    end
    local hx, hy = spec.hub[1], spec.hub[2]
    graphics2d.drawCircle(hx, hy, 34, '#FF37474F', {layer = 3})
    for blade = 0, 2 do
        local angle = fan.spin + blade * math.pi * 2 / 3
        graphics2d.drawLine(hx, hy, hx + math.cos(angle) * 30, hy + math.sin(angle) * 30, 8, field.enabled and '#FFB0BEC5' or '#FF607D8B', {layer = 4})
    end
end

function Wind:draw(area)
    parts.drawAll(self.statics)
    for _, fan in ipairs(self.fans) do
        self:drawFan(fan)
    end
    if self.vortex.enabled then
        for ring = 1, 3 do
            local start = self.time * (1 + ring * 0.4)
            graphics2d.drawArc(kVortex[1], kVortex[2], kVortex[3] * ring / 3.2, 3, start, start + 2, '#55FFFFFF', {layer = -1})
        end
    end
    for _, body in ipairs(self.bodies) do
        if body.data.balloon then
            graphics2d.drawLine(body.x, body.y + 26, body.x - body.velocity.x * 0.05, body.y + 80, 2, '#99FFFFFF')
        end
    end
    parts.drawAll(self.bodies, {layer = 1})
    parts.drawAll(self.leaves, {layer = 1})
    self.grab:draw()
end

return Wind
