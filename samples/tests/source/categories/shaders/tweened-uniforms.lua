-- Uniforms driven by tweens: tweens animate plain fields, and the scene hands the fields to the materials before it draws.
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local m = require('haylen.math')
local tween = require('haylen.tween')
local ui = require('haylen.ui')

local ShaderTest = require('categories.shaders.shader-test')

local TweenedUniforms = haylen.class('TweenedUniforms', ShaderTest)

TweenedUniforms.shown = {
    {'Yoyo dissolve', 'planet', 'dissolve'},
    {'Outline color in HSV', 'hero', 'outline'},
    {'Elastic wave', 'planet', 'wave'},
    {'Stepped pixelation', 'planet', 'pixelate'},
}

function TweenedUniforms:init(entry)
    TweenedUniforms.super.init(self, entry)
    self.images = {hero = ShaderTest.hero(), planet = ShaderTest.planet()}
    local hero = self.images.hero
    self.values = {dissolve = 0, glow = m.color('#FFFF4040'), amplitude = 0, blocks = 64, flash = 0, reveal = 0}
    self.materials = {
        dissolve = ShaderTest.material('dissolve', {edge_color = '#FF60E0FF', edge_width = 0.1, noise_scale = 1.5, noise_texture = ShaderTest.noise()}),
        outline = ShaderTest.material('outline', {texel = {1 / hero.width, 1 / hero.height}, thickness = 1.5}),
        wave = ShaderTest.material('wave', {frequency = 14, speed = 4}),
        pixelate = ShaderTest.material('pixelate'),
        flash = ShaderTest.material('flash', {flash_color = '#FFFFFFFF'}),
        timeline = ShaderTest.material('dissolve', {edge_color = '#FFFFD040', edge_width = 0.12, noise_scale = 1, noise_texture = ShaderTest.noise()}),
    }
    self.cells = ShaderTest.cells(5, 3)
end

function TweenedUniforms:enter()
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
    self:frame{
        hint = 'Each value on screen comes from a tween. Replay the timeline to watch the hero dissolve, flash and come back.',
        controls = {ui.button{id = 'replay', text = 'Replay the timeline', onClick = function()
            self.timeline:restart()
        end}},
        focus = 'replay',
    }
end

function TweenedUniforms:update(dt)
    TweenedUniforms.super.update(self, dt)
    local values, materials = self.values, self.materials
    materials.dissolve:set('amount', values.dissolve)
    materials.outline:set('outline_color', values.glow)
    materials.wave:set('time', haylen.elapsed())
    materials.wave:set('amplitude', values.amplitude)
    materials.pixelate:set('blocks', {values.blocks, values.blocks})
    materials.timeline:set('amount', values.reveal)
    materials.flash:set('amount', values.flash)
    self:status(string.format('Dissolve %.2f   Outline "%s"   Amplitude %.3f   Blocks %d   Timeline %.2f', values.dissolve, values.glow:toHex(), values.amplitude, values.blocks, self.timeline.progress))
end

function TweenedUniforms:draw(area)
    for index, entry in ipairs(TweenedUniforms.shown) do
        local cell = self.cells[index]
        graphics2d.draw(self.images[entry[2]], cell.x, cell.y, {width = cell.size, height = cell.size, material = self.materials[entry[3]]})
        ShaderTest.caption(entry[1], cell.x, cell.y + cell.size / 2 + 8)
    end

    -- The timeline drives two materials, so the hero draws twice: the dissolve first and the flash over it.
    local cell, hero = self.cells[5], self.images.hero
    graphics2d.draw(hero, cell.x, cell.y, {width = cell.size, height = cell.size, material = self.materials.timeline})
    graphics2d.draw(hero, cell.x, cell.y, {width = cell.size, height = cell.size, material = self.materials.flash, color = {1, 1, 1, self.values.flash}, layer = 1})
    ShaderTest.caption('Timeline', cell.x, cell.y + cell.size / 2 + 8)
end

return TweenedUniforms
