-- Uniforms driven by tweens: tweens animate plain fields, and the scene hands the fields to the materials before it draws.
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local m = require('haylen.math')
local tween = require('haylen.tween')
local ui = require('haylen.ui')

local art = require('art')
local sample = require('sample')

local TweenedUniforms = haylen.class('TweenedUniforms', sample.Test)

TweenedUniforms.hints = 'Each value on screen comes from a tween. Replay the timeline to watch the hero dissolve, flash and come back.'

function TweenedUniforms:init(entry)
    TweenedUniforms.super.init(self, entry)
    self.hero = art.hero()
    self.planet = art.planet()
    self.values = {dissolve = 0, glow = m.color('#FFFF4040'), amplitude = 0, blocks = 64, flash = 0, reveal = 0}
    self.materials = {
        dissolve = art.material('dissolve', {edge_color = '#FF60E0FF', edge_width = 0.1, noise_scale = 1.5, noise_texture = art.noise()}),
        outline = art.material('outline', {texel = {1 / self.hero.width, 1 / self.hero.height}, thickness = 1.5}),
        wave = art.material('wave', {frequency = 14, speed = 4}),
        pixelate = art.material('pixelate'),
        flash = art.material('flash', {flash_color = '#FFFFFFFF'}),
        timeline = art.material('dissolve', {edge_color = '#FFFFD040', edge_width = 0.12, noise_scale = 1, noise_texture = art.noise()}),
    }
end

function TweenedUniforms:enter()
    TweenedUniforms.super.enter(self)
    local values = self.values
    tween.to(values, 1.6, {dissolve = 1}, {ease = 'sineInOut', loopMode = 'yoyo', repeatCount = -1, owner = self})
    tween.to(values, 3, {glow = '#FF4080FF'}, {colorSpace = 'hsv', loopMode = 'yoyo', repeatCount = -1, owner = self})
    tween.to(values, 1.2, {amplitude = 0.05}, {ease = {curve = 'elasticOut', amplitude = 1.2, period = 0.3}, loopMode = 'yoyo', repeatCount = -1, repeatDelay = 0.4, owner = self})
    tween.to(values, 2, {blocks = 6}, {ease = {steps = 8}, loopMode = 'yoyo', repeatCount = -1, integers = {'blocks'}, owner = self})
    self.timeline = tween.timeline({autoKill = false, owner = self})
        :append(tween.to(values, 0.8, {reveal = 1}, {ease = 'quadIn'}))
        :append(tween.to(values, 0.15, {flash = 1}))
        :append(tween.to(values, 0.8, {reveal = 0}, {ease = 'quadOut'}))
        :join(tween.to(values, 0.5, {flash = 0}))
        :append(0.6)
end

function TweenedUniforms:controls()
    return {ui.button{text = 'Replay the timeline', onClick = function()
        self.timeline:restart()
    end}}
end

function TweenedUniforms:update(dt)
    local values, materials = self.values, self.materials
    materials.dissolve:set('amount', values.dissolve)
    materials.outline:set('outline_color', values.glow)
    materials.wave:set('time', haylen.elapsed())
    materials.wave:set('amplitude', values.amplitude)
    materials.pixelate:set('blocks', {values.blocks, values.blocks})
    materials.timeline:set('amount', values.reveal)
    materials.flash:set('amount', values.flash)
    self:setStatus(string.format('Dissolve %.2f, outline %s, amplitude %.3f, blocks %d, timeline %.2f', values.dissolve, values.glow:toHex(), values.amplitude, values.blocks, self.timeline.progress))
end

function TweenedUniforms:render()
    graphics2d.beginScreen()
    local cells = art.cells(5, 3)
    local shown = {
        {'Yoyo dissolve', self.planet, 'dissolve'},
        {'Outline color in HSV', self.hero, 'outline'},
        {'Elastic wave', self.planet, 'wave'},
        {'Stepped pixelation', self.planet, 'pixelate'},
    }
    for index, entry in ipairs(shown) do
        local cell = cells[index]
        graphics2d.draw(entry[2], cell.x, cell.y, {width = cell.size, height = cell.size, material = self.materials[entry[3]]})
        art.caption(entry[1], cell.x, cell.y + cell.size / 2 + 8)
    end

    -- The timeline drives two materials, so the hero draws twice: the dissolve first and the flash over it.
    local cell = cells[5]
    graphics2d.draw(self.hero, cell.x, cell.y, {width = cell.size, height = cell.size, material = self.materials.timeline})
    graphics2d.draw(self.hero, cell.x, cell.y, {width = cell.size, height = cell.size, material = self.materials.flash, color = {1, 1, 1, self.values.flash}, layer = 1})
    art.caption('Timeline', cell.x, cell.y + cell.size / 2 + 8)
end

return TweenedUniforms
