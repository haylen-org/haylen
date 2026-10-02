-- Shaders on text: text draws with the text program of a material, where `haylen_base` returns the glyph with its fill and outline, so one shader serves sprites and text.
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')

local art = require('art')
local sample = require('sample')

local TextShader = haylen.class('TextShader', sample.Test)

TextShader.hints = 'Click, tap, E or the X button flashes the last line.'

function TextShader:init(entry)
    TextShader.super.init(self, entry)
    self.rainbow = art.material('rainbow', {spread = 0.0015})
    self.dissolve = art.material('dissolve', {edge_color = '#FFFF7020', edge_width = 0.05, noise_scale = 6, noise_texture = art.noise()})
    self.flash = art.material('flash', {flash_color = '#FFFFFFFF'})
    self.hit = 0
end

function TextShader:update(dt)
    if sample.pressed() then
        self.hit = 1
    end
    self.hit = math.max(0, self.hit - dt * 2.5)
    self.rainbow:set('time', haylen.elapsed())
    self.dissolve:set('amount', 0.5 + 0.5 * math.sin(haylen.elapsed() * 0.8))
    self.flash:set('amount', self.hit)
    self:setStatus(string.format('Dissolve amount %.2f, flash %.2f', self.dissolve:get('amount'), self.hit))
end

function TextShader:render()
    graphics2d.beginScreen()
    graphics2d.drawText(nil, 'Haylen', 730, 400, {size = 200, anchor = {0.5, 0.5}, outlineWidth = 6, outlineColor = '#FF101018', material = self.rainbow})
    graphics2d.drawText(nil, 'Words that burn away and come back', 730, 620, {size = 64, anchor = {0.5, 0.5}, color = '#FFFFE0B0', material = self.dissolve})
    graphics2d.drawText(nil, 'Hit flash on text', 730, 800, {size = 72, anchor = {0.5, 0.5}, color = '#FFE05040', outlineWidth = 4, outlineColor = '#FF200808', material = self.flash})
end

return TextShader
