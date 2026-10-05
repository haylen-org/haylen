-- Text: sizes, a color, an outline and a blurred shadow, a paragraph aligned and wrapped in a box, anchors around one point, rotation, a box from `measureText` and a line of rich text markup.
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local ui = require('haylen.ui')

local SpriteTest = require('categories.sprites.sprite-test')
local Test = require('harness.test')

local Text = haylen.class('Text', SpriteTest)

Text.aligns = {{id = 'left', text = 'Left'}, {id = 'center', text = 'Center'}, {id = 'right', text = 'Right'}, {id = 'fill', text = 'Fill'}}
Text.anchors = {{0, 0}, {1, 0}, {0, 1}, {1, 1}}
Text.paragraph = 'Text wraps whole words at the width of its box, and every line takes the alignment of the block. Fill stretches the spaces of each wrapped line to reach both edges.'

Text.code = [[
graphics2d.drawText(nil, 'Victory', x, y, {size = 72, color = '#FFFFE070', outlineWidth = 4, shadowOffset = {6, 6}, shadowColor = '#A0000000', shadowBlur = 6})
graphics2d.drawText(nil, paragraph, x, y, {size = 26, maxWidth = 420, align = 'fill', lineSpacing = 1.3})
local width, height = graphics2d.measureText(nil, 'Measured', {size = 48})  -- The box "drawText" fills.]]

function Text:enter()
    self.align, self.width, self.time = 'left', 420, 0
    self:frame{
        code = Text.code,
        hint = 'Change the alignment and the width of the paragraph box.',
        controls = {
            ui.formField{label = 'Align', ui.segmentedControl{id = 'align', items = Text.aligns, selected = 'left', onChange = function(event) self.align = event.value end}},
            ui.formField{label = 'Box width', ui.slider{id = 'width', min = 200, max = 520, value = 420, step = 10, showValue = true, decimals = 0, onChange = function(event) self.width = event.value end}},
        },
        focus = 'align',
    }
end

function Text:update(dt)
    Text.super.update(self, dt)
    self.time = self.time + dt
    self:status(string.format('Align "%s"   Box width %.0f', self.align, self.width))
end

function Text:draw(area)
    local x = 30
    for _, size in ipairs({18, 28, 44, 64}) do
        local line = 'Size ' .. size
        graphics2d.drawText(nil, line, x, 20, {size = size, color = Test.ink})
        x = x + graphics2d.measureText(nil, line, {size = size}) + 30
    end

    graphics2d.drawText(nil, 'Color', 30, 110, {size = 56, color = Test.warm})
    graphics2d.drawText(nil, 'Outline', 220, 110, {size = 56, color = '#FFFFFFFF', outlineWidth = 4, outlineColor = '#FF3A66E0'})
    graphics2d.drawText(nil, 'Shadow', 460, 110, {size = 56, color = '#FFFFE070', shadowOffset = {6, 6}, shadowColor = '#C0000000', shadowBlur = 6})

    local style = {size = 26, color = Test.ink, maxWidth = self.width, align = self.align, lineSpacing = 1.3}
    local _, paragraphHeight = graphics2d.measureText(nil, Text.paragraph, style)
    graphics2d.drawRectOutline({20, 200, self.width + 20, paragraphHeight + 20}, 2, Test.line)
    graphics2d.drawText(nil, Text.paragraph, 30, 210, style)

    -- Each anchor puts another corner of the block next to the same point, a few units away from the cross through it.
    local pointX, pointY = area.width * 0.72, 250
    graphics2d.drawLine(pointX - 200, pointY, pointX + 200, pointY, 1, Test.line)
    graphics2d.drawLine(pointX, pointY - 50, pointX, pointY + 50, 1, Test.line)
    graphics2d.drawCircle(pointX, pointY, 6, Test.red, {layer = 1})
    for _, anchor in ipairs(Text.anchors) do
        local x, y = pointX + (0.5 - anchor[1]) * 12, pointY + (0.5 - anchor[2]) * 12
        Test.caption(string.format('Anchor %g, %g', anchor[1], anchor[2]), x, y, {size = 24, anchor = anchor})
    end

    graphics2d.drawText(nil, 'Rotation', area.width * 0.72, area.height * 0.55, {size = 40, color = Test.green, anchor = {0.5, 0.5}, rotation = self.time})

    local width, height = graphics2d.measureText(nil, 'Measured', {size = 48})
    local left, top = area.width * 0.72 - width / 2, area.height * 0.72
    graphics2d.drawRect({left - 10, top - 6, width + 20, height + 12}, Test.line)
    graphics2d.drawText(nil, 'Measured', left, top, {size = 48, color = Test.ink, layer = 1})
    Test.caption(string.format('Size %.0f x %.0f', width, height), area.width * 0.72, top + height + 26, {size = 20, anchor = {0.5, 0.5}})

    graphics2d.drawRichText('[b]Rich[/b] text with [color=gold]colors[/color] and [wave]waves[/wave]', area.width * 0.55, area.height - 60, {size = 32})
end

return Text
