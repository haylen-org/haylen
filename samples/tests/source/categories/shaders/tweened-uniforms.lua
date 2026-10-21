-- Uniforms driven by tweens: tweens animate plain fields, and the scene hands the fields to the materials before it draws. Every card shows the value of its tween on a bar under its title.
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local m = require('haylen.math')
local tween = require('haylen.tween')
local ui = require('haylen.ui')

local ShaderTest = require('categories.shaders.shader-test')

local TweenedUniforms = haylen.class('TweenedUniforms', ShaderTest)

TweenedUniforms.code = [[
local values = {dissolve = 0, glow = m.color('#FFFF4040')}
tween.to(values, 1.6, {dissolve = 1}, {ease = 'sineInOut', loopMode = 'yoyo', repeatCount = -1, owner = self})
tween.to(values, 3, {glow = '#FF4080FF'}, {colorSpace = 'hsv', loopMode = 'yoyo', repeatCount = -1, owner = self})
dissolve:set('amount', values.dissolve)  -- Every frame, before the draw.
outline:set('outline_color', values.glow)]]

-- Each cell with its title, its picture, its material and the tweened value its bar shows, from 0 to 1.
TweenedUniforms.shown = {
    {'Yoyo dissolve', 'crate', 'dissolve', 'dissolve'},
    {'Outline color in HSV', 'walker', 'outline', 'hue'},
    {'Elastic wave', 'flag', 'wave', 'wave'},
    {'Stepped pixelation', 'planet', 'pixelate', 'steps'},
    {'Eased hit flash', 'dragon', 'flash', 'hit'},
    {'Timeline', 'knight', 'timeline', 'reveal'},
}

function TweenedUniforms:init(entry)
    TweenedUniforms.super.init(self, entry)
    self.values = {dissolve = 0, glow = m.color('#FFFF4040'), amplitude = 0, blocks = 64, hit = 0, flash = 0, reveal = 0}
    self.materials = {
        dissolve = ShaderTest.material('dissolve', {edge_color = '#FF60E0FF', edge_width = 0.12, noise_scale = 1.5, noise_texture = ShaderTest.noise()}),
        outline = ShaderTest.material('outline', {texel = ShaderTest.texel('walker'), thickness = 5}),
        wave = ShaderTest.material('wave', {frequency = 12, speed = 4}),
        pixelate = ShaderTest.material('pixelate'),
        flash = ShaderTest.material('flash', {flash_color = '#FFFFFFFF'}),
        timeline = ShaderTest.material('dissolve', {edge_color = '#FFFFD040', edge_width = 0.12, noise_scale = 1, noise_texture = ShaderTest.noise()}),
        timelineFlash = ShaderTest.material('flash', {flash_color = '#FFFFFFFF'}),
    }
    self.cells = ShaderTest.cells(#TweenedUniforms.shown, 3)
end

function TweenedUniforms:enter()
    local values = self.values
    tween.to(values, 1.6, {dissolve = 1}, {ease = 'sineInOut', loopMode = 'yoyo', repeatCount = -1, owner = self})
    tween.to(values, 3, {glow = '#FF4080FF'}, {colorSpace = 'hsv', loopMode = 'yoyo', repeatCount = -1, owner = self})
    tween.to(values, 1.2, {amplitude = 0.05}, {ease = {curve = 'elasticOut', amplitude = 1.2, period = 0.3}, loopMode = 'yoyo', repeatCount = -1, repeatDelay = 0.4, owner = self})
    tween.to(values, 2, {blocks = 6}, {ease = {steps = 8}, loopMode = 'yoyo', repeatCount = -1, integers = {'blocks'}, owner = self})
    tween.to(values, 0.9, {hit = 1}, {ease = 'expoOut', loopMode = 'yoyo', repeatCount = -1, repeatDelay = 0.5, owner = self})
    self.timeline = tween.timeline({autoKill = false, owner = self, repeatCount = -1})
        :append(tween.to(values, 0.8, {reveal = 0.9}, {ease = 'quadIn'}))
        :append(tween.to(values, 0.15, {flash = 1}))
        :append(tween.to(values, 0.8, {reveal = 0}, {ease = 'quadOut'}))
        :join(tween.to(values, 0.5, {flash = 0}))
        :append(0.8)
    self:frame{
        code = TweenedUniforms.code,
        hint = 'Each value on screen comes from a tween. Replay the timeline to watch the knight dissolve, flash and come back.',
        controls = {ui.button{id = 'replay', text = 'Replay the timeline', onClick = function()
            self.timeline:restart()
        end}},
        focus = 'replay',
    }
end

function TweenedUniforms:update(dt)
    TweenedUniforms.super.update(self, dt)
    local values, materials = self.values, self.materials
    materials.dissolve:set('amount', values.dissolve * 0.95)
    materials.outline:set('outline_color', values.glow)
    materials.wave:set('time', haylen.elapsed())
    materials.wave:set('amplitude', values.amplitude)
    materials.pixelate:set('blocks', {values.blocks, values.blocks})
    materials.flash:set('amount', 1 - values.hit)
    materials.timeline:set('amount', values.reveal)
    materials.timelineFlash:set('amount', values.flash)
    self:status(string.format('Dissolve %.2f, outline "%s", amplitude %.3f, %d blocks, flash %.2f and the timeline at %.2f.', values.dissolve, values.glow:toHex(), values.amplitude, values.blocks, 1 - values.hit, self.timeline.progress))
end

-- The value of each bar as a share from 0 to 1.
function TweenedUniforms:share(key)
    local values = self.values
    if key == 'hue' then
        return (values.glow:toHsv())
    elseif key == 'wave' then
        return values.amplitude / 0.05
    elseif key == 'steps' then
        return (values.blocks - 6) / 58
    elseif key == 'hit' then
        return 1 - values.hit
    end
    return values[key]
end

function TweenedUniforms:drawBar(cell, share)
    local rect = cell.rect
    local width = rect[3] * 0.5
    local bar = {rect[1] + rect[3] / 2 - width / 2, rect[2] + 16, width, 16}
    graphics2d.drawShape(bar, {radius = 8, color = '#CC15172A', layer = 9})
    local fill = math.max(10, (width - 6) * math.max(0, math.min(1, share)))
    graphics2d.drawShape({bar[1] + 3, bar[2] + 3, fill, 10}, {radius = 5, color = '#FFFFC93C', layer = 10})
end

function TweenedUniforms:draw(area)
    local idle = math.floor(haylen.elapsed() * 6) % 4
    for index, entry in ipairs(TweenedUniforms.shown) do
        local cell = self.cells[index]
        self:drawCard(cell, entry[1])
        local options = {material = self.materials[entry[3]]}
        if entry[2] == 'walker' then
            options.source = {idle * 256, 0, 256, 256}
        elseif entry[2] == 'dragon' then
            options.source = {math.floor(haylen.elapsed() * 3) % 2 * 256, 0, 256, 256}
        end
        self:drawPicture(cell, entry[2], cell.size * (entry[2] == 'planet' and 0.95 or 1.1), options)
        self:drawBar(cell, self:share(entry[4]))
    end

    -- The timeline drives two materials, so the knight draws twice: the dissolve first and the flash over it.
    local cell = self.cells[#TweenedUniforms.shown]
    self:drawPicture(cell, 'knight', cell.size * 1.1, {material = self.materials.timelineFlash, color = {1, 1, 1, self.values.flash}, layer = 5})
end

return TweenedUniforms
