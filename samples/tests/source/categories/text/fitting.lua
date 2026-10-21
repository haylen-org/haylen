-- Fitting text in a box: a style whose "minScale" is below 1 draws text that does not fit its box smaller, down to that share of its size. A name that does not wrap shrinks until it fits the width of its plate, and a story that wraps shrinks until its lines fit the height of its box.
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local ui = require('haylen.ui')

local Test = require('harness.test')
local TextTest = require('categories.text.text-test')
local fonts = require('categories.text.fonts')

local Fitting = haylen.class('Fitting', TextTest)

Fitting.code = [[
local name = 'Captain Marigold of the Northern Sea'
graphics2d.drawText(font, name, x, y, {size = 56, maxWidth = 460, wrap = false, minScale = 0.4, anchor = {0.5, 0.5}})
graphics2d.drawText(font, story, x, y, {size = 40, maxWidth = 460, maxHeight = 200, minScale = 0.4})
graphics2d.drawText(font, story, x, y, {size = 40, maxWidth = 460})]]

Fitting.name = 'Captain Marigold of the Northern Sea'
Fitting.maxWidth = 620
Fitting.padding = 20
Fitting.story = 'On the third night the keeper climbed the lighthouse stairs, lit the great lamp and watched the ships find the harbor, one sail after another, until the last boat was home.'

function Fitting:init(entry)
    Fitting.super.init(self, entry)
    self.width = 500
    self.height = 240
    self.minScale = 0.4
    self.sweep = true
    self.latin = fonts.load('crimson')
end

function Fitting:enter()
    self:frame{
        code = Fitting.code,
        page = {1340, 660},
        hint = 'Drag the sliders, or focus them and press left and right. The boxes are the space the text has inside their padding, and with sweep on they change size by themselves.',
        controls = {
            ui.formField{label = 'Width of the boxes', ui.slider{id = 'width', min = 360, max = Fitting.maxWidth, step = 10, value = self.width, showValue = true, decimals = 0, onChange = function(event)
                self.width = event.value
                self:stopSweep()
            end}},
            ui.formField{label = 'Height of the story', ui.slider{id = 'height', min = 120, max = 360, step = 10, value = self.height, showValue = true, decimals = 0, onChange = function(event)
                self.height = event.value
                self:stopSweep()
            end}},
            ui.formField{label = 'Smallest scale', ui.slider{id = 'minScale', min = 0.2, max = 1, step = 0.05, value = self.minScale, showValue = true, decimals = 2, onChange = function(event) self.minScale = event.value end}},
            ui.toggle{id = 'sweep', text = 'Sweep the boxes', checked = self.sweep, onChange = function(event) self.sweep = event.checked end},
        },
        focus = 'width',
    }
end

function Fitting:stopSweep()
    self.sweep = false
    self:set('sweep', {checked = false})
end

function Fitting:nameStyle()
    return {size = 56, maxWidth = self.width - Fitting.padding * 2, wrap = false, minScale = self.minScale, anchor = {0.5, 0.5}, color = Test.ink}
end

function Fitting:storyStyle()
    return {size = 40, maxWidth = self.width - Fitting.padding * 2, maxHeight = self.height - Fitting.padding * 2, minScale = self.minScale, color = Test.ink}
end

function Fitting:update(dt)
    Fitting.super.update(self, dt)
    if self.sweep then
        local time = haylen.elapsed()
        self.width = 490 + 130 * math.sin(time * 0.6)
        self.height = 240 + 120 * math.sin(time * 0.45)
    end
    local nameWidth = graphics2d.measureText(self.latin, Fitting.name, self:nameStyle())
    local _, storyHeight = graphics2d.measureText(self.latin, Fitting.story, self:storyStyle())
    self:status(string.format('The boxes are %.0f by %.0f, the name takes %.0f of their width and the story %.0f of their height, shrinking to no less than %.2f of their size.', self.width, self.height, nameWidth, storyHeight, self.minScale))
end

function Fitting:draw(area)
    local layout, padding = self.layout, Fitting.padding
    local width, height = self.width, self.height
    TextTest.label('A name that does not wrap fits the width of its plate', 0, 0)
    local plate = {0, 44, width, 100}
    graphics2d.drawShape(plate, {radius = 14, color = '#FF2A3550', borderWidth = 3, borderColor = '#FFFFD54F'})
    graphics2d.drawText(self.latin, Fitting.name, width / 2, plate[2] + plate[4] / 2, self:nameStyle())

    local storyY = 224
    TextTest.label('A story that wraps fits the height of its box', 0, storyY - 44)
    graphics2d.drawShape({0, storyY, width, height}, {radius = 14, color = '#FF22303A', borderWidth = 3, borderColor = '#FF6FDCA0'})
    graphics2d.drawText(self.latin, Fitting.story, padding, storyY + padding, self:storyStyle())

    -- The same story without a smallest scale runs past the bottom of its box, and the page keeps room for it.
    local right = layout.width - Fitting.maxWidth
    TextTest.label('Without a smallest scale, it overflows its box', right, 0)
    graphics2d.drawShape({right, 44, width, height}, {radius = 14, color = '#FF22303A', borderWidth = 3, borderColor = '#FFFF8A84'})
    graphics2d.drawText(self.latin, Fitting.story, right + padding, 44 + padding, {size = 40, maxWidth = width - padding * 2, color = Test.ink, layer = 1})
end

return Fitting
