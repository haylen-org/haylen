-- Blend modes: the same emitter with each blend of the renderer, rising from a dark half into a light half, so each column shows how its blend treats both. Additive and screen brighten, multiply darkens and vanishes over black, and alpha covers. Only the premultiplied blend takes colors already multiplied by their alpha, so that column uses a premultiplied image and colors.
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local particles2d = require('haylen.particles2d')

local ParticleTest = require('categories.particles.particle-test')

local BlendModes = haylen.class('BlendModes', ParticleTest)

BlendModes.code = [[
local glow = particles2d.newEmitter({texture = dot, rate = 40, blend = 'additive'})
glow:configure({colors = {'#FFFF9040', '#80FF5020', '#00FF2010'}})
glow:configure({blend = 'multiply'})  -- Or "alpha", "screen" and "premultiplied".
-- Premultiplied blending takes colors already multiplied by their alpha.
local soft = particles2d.newEmitter({texture = premultipliedDot, blend = 'premultiplied'})
soft:configure({colors = {'#FFFF9040', '#80802810', '#00000000'}})]]

BlendModes.modes = {'alpha', 'additive', 'multiply', 'screen', 'premultiplied'}
BlendModes.titles = {alpha = 'Covers', additive = 'Adds light', multiply = 'Darkens', screen = 'Lightens softly', premultiplied = 'Premultiplied'}
BlendModes.straight = {piece = 'soft-dot', colors = {'#FFFF9040', '#80FF5020', '#00FF2010'}}
BlendModes.premultiplied = {piece = 'soft-dot-premultiplied', colors = {'#FFFF9040', '#80802810', '#00000000'}}

function BlendModes:init(entry)
    BlendModes.super.init(self, entry)
    self.emitters = {}
    for index, mode in ipairs(BlendModes.modes) do
        local look = mode == 'premultiplied' and BlendModes.premultiplied or BlendModes.straight
        local emitter = particles2d.newEmitter(ParticleTest.look(look.piece, {rate = 40, lifetime = {2, 2.4}, speed = {200, 260}, direction = -1.5708, spread = 0.3, startSize = {60, 80}, endSize = {24, 34}, colors = look.colors, blend = mode, layer = 2, seed = 80 + index}))
        emitter.position = {(index - 3) * 340, 260}
        self.emitters[index] = emitter
    end
end

function BlendModes:enter()
    self:frame{hint = 'Each column blends the same orange particles its own way as they rise from the dark half into the light one. Multiply only darkens, so over black it disappears.', code = BlendModes.code}
end

function BlendModes:update(dt)
    BlendModes.super.update(self, dt)
    for _, emitter in ipairs(self.emitters) do
        emitter:update(dt)
    end
    self:status('Five blends of the same particles, from left to right: alpha, additive, multiply, screen and premultiplied.')
end

function BlendModes:draw(area)
    local view = graphics2d.canvasBounds()
    graphics2d.drawRect({view.x, view.y, view.width, -view.y}, '#FFC8D0E0')
    graphics2d.drawRect({view.x, 0, view.width, view:bottom()}, '#FF141820')
    for index, emitter in ipairs(self.emitters) do
        emitter:draw()
        local mode = BlendModes.modes[index]
        ParticleTest.label(BlendModes.titles[mode], emitter.x, 300, string.format("blend = '%s'", mode))
    end
end

return BlendModes
