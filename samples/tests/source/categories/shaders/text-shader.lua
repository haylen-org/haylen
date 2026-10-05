-- Shaders on text: text draws with the text program of a material, where `haylen_base` returns the glyph with its fill and outline, so one shader serves sprites and text.
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')

local ShaderTest = require('categories.shaders.shader-test')

local TextShader = haylen.class('TextShader', ShaderTest)

function TextShader:init(entry)
    TextShader.super.init(self, entry)
    self.rainbow = ShaderTest.material('rainbow', {spread = 0.0015})
    self.dissolve = ShaderTest.material('dissolve', {edge_color = '#FFFF7020', edge_width = 0.05, noise_scale = 6, noise_texture = ShaderTest.noise()})
    self.flash = ShaderTest.material('flash', {flash_color = '#FFFFFFFF'})
    self.hit = 0
end

function TextShader:enter()
    self:frame{hint = 'A click or a tap on the play area, E, Enter or the south button flashes the last line.', presses = true}
end

function TextShader:update(dt)
    TextShader.super.update(self, dt)
    if self:pressed() then
        self.hit = 1
    end
    self.hit = math.max(0, self.hit - dt * 2.5)
    self.rainbow:set('time', haylen.elapsed())
    self.dissolve:set('amount', 0.5 + 0.5 * math.sin(haylen.elapsed() * 0.8))
    self.flash:set('amount', self.hit)
    self:status(string.format('Dissolve amount %.2f   Flash %.2f', self.dissolve:get('amount'), self.hit))
end

function TextShader:draw(area)
    local x = ShaderTest.layout[1] / 2
    graphics2d.drawText(nil, 'Haylen', x, 180, {size = 200, anchor = {0.5, 0.5}, outlineWidth = 6, outlineColor = '#FF101018', material = self.rainbow})
    graphics2d.drawText(nil, 'Words that burn away and come back', x, 400, {size = 64, anchor = {0.5, 0.5}, color = '#FFFFE0B0', material = self.dissolve})
    graphics2d.drawText(nil, 'Hit flash on text', x, 580, {size = 72, anchor = {0.5, 0.5}, color = '#FFE05040', outlineWidth = 4, outlineColor = '#FF200808', material = self.flash})
end

return TextShader
