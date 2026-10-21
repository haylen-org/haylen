-- Shaders on text: text draws with the text program of a material, where `haylen_base` returns the glyph with its fill and outline, so one shader serves sprites and text.
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')

local ShaderTest = require('categories.shaders.shader-test')

local TextShader = haylen.class('TextShader', ShaderTest)

TextShader.code = [[
local dissolve = graphics2d.newMaterial(assets.shader('shaders/dissolve.shader'), {edge_width = 0.05, noise_scale = 6})
dissolve:set('edge_color', '#FFFF7020')
dissolve:set('noise_texture', noise)
dissolve:set('amount', 0.5 + 0.5 * math.sin(haylen.elapsed() * 0.8))
graphics2d.drawText(nil, 'Words that burn away and come back', x, y, {size = 64, material = dissolve})
graphics2d.drawText(font, 'Isle of Tests', x, y, {size = 170, outlineWidth = 8, material = rainbow})]]

function TextShader:init(entry)
    TextShader.super.init(self, entry)
    self.rainbow = ShaderTest.material('rainbow', {spread = 0.0012})
    self.dissolve = ShaderTest.material('dissolve', {edge_color = '#FFFF7020', edge_width = 0.05, noise_scale = 6, noise_texture = ShaderTest.noise()})
    self.flash = ShaderTest.material('flash', {flash_color = '#FFFFFFFF'})
    self.hit = 0
end

function TextShader:enter()
    self:frame{code = TextShader.code, hint = 'A click or a tap on the play area, E, Enter or the south button flashes the last line.', presses = true}
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
    self:status(string.format('The words are %d%% burned away, and the flash is at %.2f.', math.floor(self.dissolve:get('amount') * 100), self.hit))
end

-- The text stands on a title screen at night, on a card that fills the layout.
function TextShader:draw(area)
    local layout = ShaderTest.layout
    self:drawCard({rect = {24, 24, layout[1] - 48, layout[2] - 48}}, 'Three materials on text', true)
    local x = layout[1] / 2
    graphics2d.drawText(self:font(), 'Isle of Tests', x, 190, {size = 170, anchor = {0.5, 0.5}, outlineWidth = 8, outlineColor = '#FF15172A', material = self.rainbow, layer = 5})
    graphics2d.drawText(nil, 'Words that burn away and come back', x, 380, {size = 64, anchor = {0.5, 0.5}, color = '#FFFFE0B0', material = self.dissolve, layer = 5})
    graphics2d.drawText(self:font(), 'Hit flash on text', x, 530, {size = 84, anchor = {0.5, 0.5}, color = '#FFF0473E', outlineWidth = 5, outlineColor = '#FF200808', material = self.flash, layer = 5})
end

return TextShader
