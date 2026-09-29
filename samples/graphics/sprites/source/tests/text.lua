-- Text: sizes, a color, an outline and a blurred shadow, a paragraph aligned and wrapped in a box, anchors around one point, rotation, a box from measureText and a line of rich text markup.
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local ui = require('haylen.ui')

local sample = require('sample')

local Text = haylen.class('Text', sample.Test)

local kAligns = {{id = 'left', text = 'Left'}, {id = 'center', text = 'Center'}, {id = 'right', text = 'Right'}, {id = 'fill', text = 'Fill'}}
local kAnchors = {{0, 0}, {1, 0}, {0, 1}, {1, 1}}
local kParagraph = 'Text wraps whole words at the width of its box, and every line takes the alignment of the block. Fill stretches the spaces of each wrapped line to reach both edges.'
local kCode = [[
graphics2d.drawText(nil, 'Victory', x, y, {size = 72, color = '#FFFFE070', outlineWidth = 4, shadowOffset = {6, 6}, shadowColor = '#A0000000', shadowBlur = 6})
graphics2d.drawText(nil, paragraph, x, y, {size = 26, maxWidth = 420, align = 'fill', lineSpacing = 1.3})
local width, height = graphics2d.measureText(nil, 'Measured', {size = 48})  -- the box drawText fills]]

function Text:enter()
    self.align, self.width, self.time = 'left', 420, 0
    self:frame({
        hint = 'Change the alignment and the width of the paragraph box.',
        code = kCode,
        controls = {
            ui.formField{label = 'Align', ui.segmentedControl{id = 'align', items = kAligns, selected = 'left', onChange = function(event) self.align = event.value end}},
            ui.formField{label = 'Box width', ui.slider{id = 'width', min = 200, max = 520, value = 420, step = 10, showValue = true, decimals = 0, onChange = function(event) self.width = event.value end}},
        },
        focus = 'align',
    })
end

function Text:update(dt)
    Text.super.update(self, dt)
    self.time = self.time + dt
    self:status(string.format('align %s   box width %.0f', self.align, self.width))
end

function Text:draw(area)
    local x = 30
    for index, size in ipairs({18, 28, 44, 64}) do
        graphics2d.drawText(nil, size .. ' units', x, 20, {size = size, color = sample.ink})
        x = x + graphics2d.measureText(nil, size .. ' units', {size = size}) + 30
    end

    graphics2d.drawText(nil, 'Color', 30, 110, {size = 56, color = sample.warm})
    graphics2d.drawText(nil, 'Outline', 220, 110, {size = 56, color = '#FFFFFFFF', outlineWidth = 4, outlineColor = '#FF3A66E0'})
    graphics2d.drawText(nil, 'Shadow', 460, 110, {size = 56, color = '#FFFFE070', shadowOffset = {6, 6}, shadowColor = '#C0000000', shadowBlur = 6})

    local style = {size = 26, color = sample.ink, maxWidth = self.width, align = self.align, lineSpacing = 1.3}
    local _, paragraphHeight = graphics2d.measureText(nil, kParagraph, style)
    graphics2d.drawRectOutline({20, 200, self.width + 20, paragraphHeight + 20}, 2, sample.line)
    graphics2d.drawText(nil, kParagraph, 30, 210, style)

    -- Each anchor puts another corner of the block on the same point.
    local pointX, pointY = area.width * 0.72, 250
    graphics2d.drawCircle(pointX, pointY, 6, sample.red, {layer = 1})
    for _, anchor in ipairs(kAnchors) do
        graphics2d.drawText(nil, string.format('anchor {%g, %g}', anchor[1], anchor[2]), pointX, pointY, {size = 24, color = sample.muted, anchor = anchor})
    end

    local spinX, spinY = area.width * 0.72, area.height * 0.55
    graphics2d.drawText(nil, 'Rotation', spinX, spinY, {size = 40, color = sample.green, anchor = {0.5, 0.5}, rotation = self.time})

    local width, height = graphics2d.measureText(nil, 'Measured', {size = 48})
    local left, top = area.width * 0.72 - width / 2, area.height * 0.72
    graphics2d.drawRect({left - 10, top - 6, width + 20, height + 12}, '#FF2C3147')
    graphics2d.drawText(nil, 'Measured', left, top, {size = 48, color = sample.ink, layer = 1})
    graphics2d.drawText(nil, string.format('%.0f x %.0f', width, height), area.width * 0.72, top + height + 26, {size = 20, color = sample.muted, anchor = {0.5, 0.5}})

    graphics2d.drawRichText('[b]Rich[/b] text with [color=gold]colors[/color] and [wave]waves[/wave]', area.width * 0.55, area.height - 60, {size = 32})
end

return Text
