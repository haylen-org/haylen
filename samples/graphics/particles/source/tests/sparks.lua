-- Sparks: a cone emitter throws small additive particles that damping slows and gravity pulls down, like a grinder on metal.
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local input = require('haylen.input')
local particles2d = require('haylen.particles2d')
local ui = require('haylen.ui')

local art = require('art')
local sample = require('sample')

local Sparks = haylen.class('Sparks', sample.Test)

Sparks.hints = 'The grinder follows the cursor. With the switch on, sparks fly only while you hold the mouse button, a finger, E or the X button.'

function Sparks:init(entry)
    Sparks.super.init(self, entry)
    self.camera = graphics2d.newCamera()
    self.cursor = sample.Cursor()
    self.held = false
    self.sparks = particles2d.newEmitter({texture = art.texture('spark'), rate = 400, lifetime = {0.4, 1.1}, speed = {500, 1100}, direction = -0.5, spread = 0.7, gravity = {0, 1400}, damping = 1.2, startSize = {5, 9}, endSize = 1, colors = {'#FFFFFFE0', '#FFFFD060', '#FFFF7010', '#00A02000'}, shape = 'cone', shapeSize = {12, 0}, blend = 'additive', maxParticles = 2048, layer = 2, seed = 8})
    self.glow = particles2d.newEmitter({texture = art.texture('soft'), rate = 30, lifetime = 0.15, speed = 0, startSize = {90, 130}, endSize = 60, colors = {'#A0FFD080', '#00FF8020'}, blend = 'additive', layer = 3, seed = 9})
end

function Sparks:controls()
    return {ui.toggle{text = 'Only while held', onChange = function(event)
        self.held = event.checked
    end}}
end

function Sparks:update(dt)
    self.cursor:update(dt)
    local x, y = self.cursor:world(self.camera)
    local touching = #input.touches() > 0 and not ui.wantsPointer()
    local on = not self.held or input.down('place') or touching
    for _, emitter in ipairs({self.sparks, self.glow}) do
        emitter.position = {x, y}
        emitter.emitting = on
        emitter:update(dt)
    end
    self:setStatus(string.format('%d sparks, emitting %s', self.sparks.count, tostring(on)))
end

function Sparks:render()
    graphics2d.beginWorld(self.camera)
    art.backdrop({0.06, 0.06, 0.08}, {0.14, 0.14, 0.16})
    graphics2d.drawRect({-1600, 420, 3200, 400}, '#FF2A2C30', {layer = 1})
    self.sparks:draw()
    self.glow:draw()
end

function Sparks:renderUi()
    graphics2d.beginScreen()
    self.cursor:draw()
end

return Sparks
