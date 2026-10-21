-- Outlines and small text: outlines wide enough to reach the letters beside them, at every size from 10 to 96, in plain and rich text and over a shadow, where every outline must stay under every letter. Beside them, small text drifts slowly, once at its exact place and once with the style option "pixelSnap", which keeps its strokes on whole pixels.
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local ui = require('haylen.ui')

local Test = require('harness.test')
local TextTest = require('categories.text.text-test')
local fonts = require('categories.text.fonts')

local OutlinesSmallText = haylen.class('OutlinesSmallText', TextTest)

OutlinesSmallText.sizes = {10, 14, 18, 24, 36, 56, 96}
OutlinesSmallText.small = {9, 11, 13, 16, 20}
OutlinesSmallText.backdrop = '#FF1A2030'
OutlinesSmallText.code = [[
local family = graphics.newFontFamily({regular = assets.font('fonts/fira-sans-regular.otf')})
graphics2d.drawText(family, 'WAVE Ty fall 17', x, y, {size = 36, outlineWidth = 5, outlineColor = '#FF203070'})
graphics2d.drawText(family, 'Size 11: Wood 12, Stone 4', x, y, {size = 11, pixelSnap = true})
local rich = graphics2d.newRichText('[outline=7 color=#FF6A1830]Rich WAVE[/outline]', {family = family, size = 48})]]

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
    self:status(string.format('Each outline is %.0f%% of the size of its text, and the small text has drifted %.2f units along its path.', self.reach * 100, self.drift % 20))
end

-- Draws the outlined lines from the smallest to the largest, spread over the height of the page.
function OutlinesSmallText:outlines(height)
    local sizes, total = OutlinesSmallText.sizes, 0
    for _, size in ipairs(sizes) do
        total = total + size * 1.25
    end
    local gap = (height - total) / (#sizes - 1)
    local y = 0
    for _, size in ipairs(sizes) do
        local width = math.max(1, size * self.reach)
        graphics2d.drawText(self.family, 'WAVE Ty fall 17', 0, y, {size = size, color = '#FFFFE070', outlineWidth = width, outlineColor = '#FF203070', shadowOffset = {size / 16, size / 16}, shadowColor = '#A0000000', shadowBlur = size / 24})
        y = y + size * 1.25 + gap
    end
end

-- Draws the small lines twice in boxes side by side, once at their exact place and once snapped to whole pixels, drifting together.
function OutlinesSmallText:smallText(x, y, width, height)
    local column = (width - 24) / 2
    local offset = self.drift % 20
    local pitch = (height - 64) / #OutlinesSmallText.small
    for index, title in ipairs({'At its exact place', 'With the option "pixelSnap"'}) do
        local left = x + (index - 1) * (column + 24)
        TextTest.label(title, left, y)
        TextTest.box({left, y + 40, column, height - 40})
        for line, size in ipairs(OutlinesSmallText.small) do
            local top = y + 56 + (line - 1) * pitch + offset * 0.37
            graphics2d.drawText(self.family, string.format('Size %d: Wood 12, Stone 4', size), left + 20 + offset, top, {size = size, pixelSnap = index == 2, color = Test.ink})
        end
    end
end

function OutlinesSmallText:draw(area)
    local layout = self.layout
    TextTest.fill(OutlinesSmallText.backdrop)
    local right = layout.width * 0.52
    self:outlines(layout.height)
    self.rich:draw(right, 0)
    TextTest.label('Rich text with an outline and a glow', right, 76)
    local top = 150
    self:smallText(right, top, layout.width - right, layout.height - top)
end

return OutlinesSmallText
