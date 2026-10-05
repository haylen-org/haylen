-- Outlines and small text: outlines wide enough to reach the letters beside them, at every size from 10 to 96, in plain and rich text and over a shadow, where every outline must stay under every letter. Below, small text drifts slowly, once at its exact place and once with the style option "pixelSnap", which keeps its strokes on whole pixels.
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local ui = require('haylen.ui')

local Test = require('harness.test')
local TextTest = require('categories.text.text-test')
local fonts = require('categories.text.fonts')

local OutlinesSmallText = haylen.class('OutlinesSmallText', TextTest)

OutlinesSmallText.sizes = {10, 14, 18, 24, 36, 56, 96}
OutlinesSmallText.small = {9, 11, 13, 16}
OutlinesSmallText.code = [[
graphics2d.drawText(nil, 'WAVE', x, y, {size = 36, outlineWidth = 5, outlineColor = '#FF203070'})
graphics2d.drawText(nil, 'Small print', x, y, {size = 11, pixelSnap = true})]]

function OutlinesSmallText:init(entry)
    OutlinesSmallText.super.init(self, entry)
    self.family = fonts.family('fira')
    self.reach = 0.14
    self.moving = true
    self.time = 0
    self.drift = 0
    self:refresh()
end

function OutlinesSmallText:enter()
    self:frame{
        code = OutlinesSmallText.code,
        hint = 'No outline may cover the letter next to it at any size or reach. The snapped column keeps the same crisp strokes as it moves, while the other one blurs and shimmers.',
        controls = {
            ui.formField{label = 'Outline reach', ui.slider{id = 'reach', min = 0.04, max = 0.3, step = 0.01, value = self.reach, showValue = true, decimals = 2, onChange = function(event)
                self.reach = event.value
                self:refresh()
            end}},
            ui.checkbox{id = 'moving', text = 'Move the small text', checked = true, onChange = function(event) self.moving = event.checked end},
        },
        focus = 'reach',
    }
end

-- The rich text takes its outline from its markup, so it follows the slider.
function OutlinesSmallText:refresh()
    self.rich = graphics2d.newRichText(string.format('[outline=%d color=#FF6A1830][glow=6 color=#FFFFC040]Rich WAVE[/glow][/outline]', math.floor(48 * self.reach + 0.5)), {family = self.family, size = 48, color = '#FFFFF0D0'})
end

function OutlinesSmallText:update(dt)
    OutlinesSmallText.super.update(self, dt)
    self.time = self.time + dt
    if self.moving then
        self.drift = self.drift + dt * 3
    end
    self.rich:update(dt)
    self:status(string.format('Outline width %.0f%% of the size   Drift %.2f units', self.reach * 100, self.drift % 20))
end

function OutlinesSmallText:draw(area)
    local layout = self.layout
    graphics2d.drawRect(layout, '#FF1A2030')

    local x, y = 30, 20
    for _, size in ipairs(OutlinesSmallText.sizes) do
        local width = math.max(1, size * self.reach)
        graphics2d.drawText(self.family, 'WAVE Ty fall 17', x, y, {size = size, color = '#FFFFE070', outlineWidth = width, outlineColor = '#FF203070', shadowOffset = {size / 16, size / 16}, shadowColor = '#A0000000', shadowBlur = size / 24})
        y = y + size * 1.25 + 4
    end
    self.rich:draw(860, 30)
    Test.caption('Rich text with an outline and a glow', 860, 100, {size = 22})

    local top, left, right = 300, 860, 1120
    local offset = self.drift % 20
    Test.caption('Exact place', left, top - 40, {size = 22, color = Test.ink})
    Test.caption('Option "pixelSnap"', right, top - 40, {size = 22, color = Test.ink})
    for index, size in ipairs(OutlinesSmallText.small) do
        local line = top + (index - 1) * 50 + offset * 0.37
        local text = string.format('Size %d: Wood 12, Stone 4', size)
        graphics2d.drawText(self.family, text, left + offset, line, {size = size})
        graphics2d.drawText(self.family, text, right + offset, line, {size = size, pixelSnap = true})
    end
end

return OutlinesSmallText
