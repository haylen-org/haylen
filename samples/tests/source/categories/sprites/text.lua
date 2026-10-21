-- Text: sizes on one baseline, a color, an outline and a blurred shadow, a paragraph aligned and wrapped in a box, anchors around one point, rotation, a box from `measureText` and a line of rich text markup, each in a card of its own. The cards fill a landscape or a portrait layout, whichever shows them larger.
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local ui = require('haylen.ui')

local Panels = require('categories.sprites.panels')
local SpriteTest = require('categories.sprites.sprite-test')
local Test = require('harness.test')

local Text = haylen.class('Text', SpriteTest)

Text.aligns = {{id = 'left', text = 'Left'}, {id = 'center', text = 'Center'}, {id = 'right', text = 'Right'}, {id = 'fill', text = 'Fill'}}
Text.alignNames = {left = 'to the left', center = 'to the center', right = 'to the right', fill = 'to both edges'}
Text.anchors = {{0, 0}, {1, 0}, {0, 1}, {1, 1}}
Text.sizes = {18, 28, 44, 64}
Text.paragraph = 'Text wraps whole words at the width of its box, and every line takes the alignment of the block. Fill stretches the spaces of each wrapped line to reach both edges.'
Text.rich = '[b]Rich[/b] text with [color=gold]colors[/color], [i]italics[/i], [outline=2 color=#F0473E]outlines[/outline] and [wave]waves[/wave].'

-- The cards of each layout in world units around the center of the view, which the camera fits into the play area.
Text.layouts = {
    landscape = {view = {1660, 780}, cards = {
        sizes = {-810, -370, 800, 170}, styles = {10, -370, 800, 170},
        paragraph = {-810, -180, 620, 550},
        anchors = {-170, -180, 480, 265}, rotation = {330, -180, 480, 265},
        measure = {-170, 105, 480, 265}, rich = {330, 105, 480, 265},
    }},
    portrait = {view = {800, 1440}, cards = {
        sizes = {-380, -700, 760, 170}, styles = {-380, -510, 760, 170},
        paragraph = {-380, -320, 760, 460},
        anchors = {-380, 160, 370, 260}, rotation = {10, 160, 370, 260},
        measure = {-380, 440, 370, 260}, rich = {10, 440, 370, 260},
    }},
}

Text.code = [[
graphics2d.drawText(nil, 'Shadow', x, y, {size = 56, shadowOffset = {6, 6}, shadowColor = '#C0000000', shadowBlur = 6})
graphics2d.drawText(nil, 'Outline', x, y, {size = 56, outlineWidth = 3, outlineColor = '#FFF0473E'})
graphics2d.drawText(nil, paragraph, x, y, {size = 28, maxWidth = 420, align = 'fill', lineSpacing = 1.35})
graphics2d.drawText(nil, 'Anchor 1, 0', x, y, {size = 24, anchor = {1, 0}})
local width, height = graphics2d.measureText(nil, 'Measured', {size = 48})
graphics2d.drawRichText('[b]Rich[/b] text with [color=gold]colors[/color]', x, y, {size = 30})]]

function Text:enter()
    self.align, self.width, self.time = 'left', 420, 0
    self.layout = Text.layouts.landscape
    self.font = graphics2d.defaultFont()
    self:frame{
        code = Text.code,
        view = self.layout.view,
        hint = 'Change the alignment and the width of the paragraph box.',
        controls = {
            ui.formField{label = 'Align', ui.segmentedControl{id = 'align', items = Text.aligns, selected = 'left', onChange = function(event) self.align = event.value end}},
            ui.formField{label = 'Box width', ui.slider{id = 'width', min = 320, max = 540, value = 420, step = 10, showValue = true, decimals = 0, onChange = function(event) self.width = event.value end}},
        },
        focus = 'align',
    }
end

-- Picks the layout that draws the cards larger in the play area.
function Text:resize(area)
    self.layout = Panels.layout(Text.layouts, area)
    self.view = self.layout.view
end

function Text:update(dt)
    Text.super.update(self, dt)
    self.time = self.time + dt
    self:status(string.format('The paragraph is aligned %s in a box %.0f units wide.', Text.alignNames[self.align], self.width))
end

function Text:drawSizes(rect)
    local inner = Panels.card(rect, 'Sizes on one baseline')
    local baseline = inner[2] + self.font:ascent(64)
    graphics2d.drawLine(inner[1], baseline, inner[1] + inner[3], baseline, 2, Test.line)
    local x = inner[1]
    for _, size in ipairs(Text.sizes) do
        local line = 'Size ' .. size
        graphics2d.drawText(nil, line, x, baseline - self.font:ascent(size), {size = size, color = Test.ink, layer = 1})
        x = x + graphics2d.measureText(nil, line, {size = size}) + 30
    end
end

function Text:drawStyles(rect)
    local inner = Panels.card(rect, 'Color, outline and shadow')
    local gap = (inner[3] - graphics2d.measureText(nil, 'ColorOutlineShadow', {size = 56})) / 2
    local x, y = inner[1], inner[2] + inner[4] / 2
    graphics2d.drawText(nil, 'Color', x, y, {size = 56, color = Test.warm, anchor = {0, 0.5}})
    x = x + graphics2d.measureText(nil, 'Color', {size = 56}) + gap
    graphics2d.drawText(nil, 'Outline', x, y, {size = 56, color = '#FFFFFFFF', outlineWidth = 3, outlineColor = '#FFF0473E', anchor = {0, 0.5}})
    x = x + graphics2d.measureText(nil, 'Outline', {size = 56}) + gap
    graphics2d.drawText(nil, 'Shadow', x, y, {size = 56, color = '#FFFFE070', shadowOffset = {6, 6}, shadowColor = '#C0000000', shadowBlur = 6, anchor = {0, 0.5}})
end

function Text:drawParagraph(rect)
    local inner = Panels.card(rect, 'Wrapping and alignment')
    local style = {size = 28, color = Test.ink, maxWidth = self.width, align = self.align, lineSpacing = 1.35, layer = 1}
    local _, height = graphics2d.measureText(nil, Text.paragraph, style)
    graphics2d.drawShape({inner[1], inner[2], self.width + 24, height + 24}, {radius = 8, color = '#FF181D2A', borderWidth = 2, borderColor = Test.accent})
    graphics2d.drawText(nil, Text.paragraph, inner[1] + 12, inner[2] + 12, style)
end

-- Each anchor puts another corner of the block next to the same point, a few units away from the cross through it.
function Text:drawAnchors(rect)
    local inner = Panels.card(rect, 'Anchors around one point')
    local x, y = inner[1] + inner[3] / 2, inner[2] + inner[4] / 2
    graphics2d.drawLine(inner[1], y, inner[1] + inner[3], y, 2, Test.line)
    graphics2d.drawLine(x, inner[2], x, inner[2] + inner[4], 2, Test.line)
    graphics2d.drawCircle(x, y, 7, Test.red, {layer = 2})
    for _, anchor in ipairs(Text.anchors) do
        local labelX, labelY = x + (0.5 - anchor[1]) * 16, y + (0.5 - anchor[2]) * 16
        graphics2d.drawText(nil, string.format('Anchor %g, %g', anchor[1], anchor[2]), labelX, labelY, {size = 24, color = Test.ink, anchor = anchor, layer = 1})
    end
end

function Text:drawRotation(rect)
    local inner = Panels.card(rect, 'Rotation')
    graphics2d.drawText(nil, 'Rotation', inner[1] + inner[3] / 2, inner[2] + inner[4] / 2, {size = 44, color = Test.green, anchor = {0.5, 0.5}, rotation = self.time * 0.8})
end

function Text:drawMeasure(rect)
    local inner = Panels.card(rect, 'Measuring')
    local width, height = graphics2d.measureText(nil, 'Measured', {size = 48})
    local left, top = inner[1] + (inner[3] - width) / 2, inner[2] + inner[4] / 2 - height / 2 - 22
    graphics2d.drawShape({left - 10, top - 6, width + 20, height + 12}, {radius = 6, color = '#FF2C3550', borderWidth = 2, borderColor = Test.accent})
    graphics2d.drawText(nil, 'Measured', left, top, {size = 48, color = Test.ink, layer = 1})
    Panels.caption(string.format('%.0f by %.0f units', width, height), inner[1] + inner[3] / 2, top + height + 22, {size = 24, color = Test.accent})
end

function Text:drawRich(rect)
    local inner = Panels.card(rect, 'Rich text')
    local options = {size = 30, maxWidth = inner[3], lineSpacing = 1.3}
    local _, height = graphics2d.measureRichText(Text.rich, options)
    graphics2d.drawRichText(Text.rich, inner[1], inner[2] + (inner[4] - height) / 2, options)
end

function Text:draw(area)
    local cards = self.layout.cards
    self:drawSizes(cards.sizes)
    self:drawStyles(cards.styles)
    self:drawParagraph(cards.paragraph)
    self:drawAnchors(cards.anchors)
    self:drawRotation(cards.rotation)
    self:drawMeasure(cards.measure)
    self:drawRich(cards.rich)
end

return Text
