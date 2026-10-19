-- The options `colorVision` and `correctColorVision` of the post-processing of a world canvas show the canvas as a deficiency of color vision sees it, or correct for it. The swatches pair colors that some deficiencies cannot tell apart.
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local ui = require('haylen.ui')

local Test = require('harness.test')

local ColorVision = haylen.class('ColorVision', Test)

ColorVision.code = [[
graphics2d.beginWorld(camera, {postProcess = {colorVision = 'deuteranopia'}})  -- Shows what that eye sees.
graphics2d.beginWorld(camera, {postProcess = {colorVision = 'protanopia', correctColorVision = true}})  -- Corrects for it.]]

ColorVision.pairs = {
    {'#FFD03B2F', '#FF3F9B3A', 'Red and green'},
    {'#FFE88A2E', '#FF8DB33A', 'Orange and lime'},
    {'#FF3A66E0', '#FF8E44C8', 'Blue and violet'},
    {'#FFF2D43A', '#FFE0E0E0', 'Yellow and white'},
    {'#FF2E9EA8', '#FF4C7DFF', 'Teal and blue'},
}

function ColorVision:enter()
    self.vision = 'normal'
    self.correct = false
    self:frame{
        hint = 'Pick a deficiency to see the swatches as it sees them, and turn the correction on to shift what it loses into colors it sees.',
        code = ColorVision.code,
        controls = {
            ui.sectionTitle{text = 'Color vision'},
            ui.radioGroup{id = 'vision', selected = 'normal', items = {{id = 'normal', text = 'Normal'}, {id = 'protanopia', text = 'Protanopia, no red cones'}, {id = 'deuteranopia', text = 'Deuteranopia, no green cones'}, {id = 'tritanopia', text = 'Tritanopia, no blue cones'}}, onChange = function(event) self.vision = event.value end},
            ui.toggle{text = 'Correct for it', onChange = function(event) self.correct = event.checked end},
        },
    }
end

function ColorVision:update(dt)
    ColorVision.super.update(self, dt)
    self:status(string.format('The canvas draws with "colorVision" set to "%s" and "correctColorVision" to %s.', self.vision, tostring(self.correct)))
end

function ColorVision:render()
    if not self.area then
        return
    end
    graphics2d.beginWorld(self.camera, {clear = Test.stageColor, postProcess = {colorVision = self.vision, correctColorVision = self.correct}})
    local area = self.area
    local rows = #ColorVision.pairs
    local height = (area.height - 80) / rows
    local width = math.min(area.width * 0.3, 320)
    for index, pair in ipairs(ColorVision.pairs) do
        local y = 40 + (index - 1) * height
        graphics2d.drawRect({60, y, width, height - 16}, pair[1])
        graphics2d.drawRect({60 + width + 12, y, width, height - 16}, pair[2])
        Test.caption(pair[3], 60 + width * 2 + 36, y + (height - 16) / 2, {anchor = {0, 0.5}, color = Test.ink})
    end
end

return ColorVision
