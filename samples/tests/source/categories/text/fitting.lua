-- Fitting text in a box: a style whose "minScale" is below 1 draws text that does not fit its box smaller, down to that share of its size. A name that does not wrap shrinks until it fits the width of its plate, and a story that wraps shrinks until its lines fit the height of its box.
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local ui = require('haylen.ui')

local Test = require('harness.test')
local TextTest = require('categories.text.text-test')
local fonts = require('categories.text.fonts')

local Fitting = haylen.class('Fitting', TextTest)

Fitting.code = [[
graphics2d.drawText(font, 'Captain Marigold of the Northern Sea', 260, 90, {size = 56, maxWidth = 520, wrap = false, minScale = 0.4, anchor = {0.5, 0.5}})
graphics2d.drawText(font, story, 0, 240, {size = 44, maxWidth = 520, maxHeight = 220, minScale = 0.4})]]

Fitting.name = 'Captain Marigold of the Northern Sea'
Fitting.story = 'On the third night the keeper climbed the lighthouse stairs, lit the great lamp and watched the ships find the harbor, one sail after another, until the last boat was home.'

function Fitting:init(entry)
    Fitting.super.init(self, entry)
    self.width = 520
    self.height = 220
    self.minScale = 0.4
    self.sweep = true
    self.latin = fonts.load('crimson')
end

function Fitting:enter()
    self:frame{
        code = Fitting.code,
        hint = 'Drag the sliders, or focus them and press left and right. The boxes are the space the text has, and with sweep on they change size by themselves.',
        controls = {
            ui.formField{label = 'Width of the boxes', ui.slider{id = 'width', min = 200, max = 640, step = 10, value = self.width, showValue = true, decimals = 0, onChange = function(event)
                self.width = event.value
                self:stopSweep()
            end}},
            ui.formField{label = 'Height of the story', ui.slider{id = 'height', min = 80, max = 400, step = 10, value = self.height, showValue = true, decimals = 0, onChange = function(event)
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
    return {size = 56, maxWidth = self.width, wrap = false, minScale = self.minScale, anchor = {0.5, 0.5}}
end

function Fitting:storyStyle()
    return {size = 44, maxWidth = self.width, maxHeight = self.height, minScale = self.minScale}
end

function Fitting:update(dt)
    Fitting.super.update(self, dt)
    if self.sweep then
        local time = haylen.elapsed()
        self.width = 420 + 200 * math.sin(time * 0.6)
        self.height = 240 + 150 * math.sin(time * 0.45)
    end
    local nameWidth = graphics2d.measureText(self.latin, Fitting.name, self:nameStyle())
    local _, storyHeight = graphics2d.measureText(self.latin, Fitting.story, self:storyStyle())
    self:status(string.format('Box %.0f by %.0f   Name %.0f wide   Story %.0f tall   Smallest scale %.2f', self.width, self.height, nameWidth, storyHeight, self.minScale))
end

function Fitting:draw(area)
    local plateY = 90
    Test.caption('A name that does not wrap fits the width of its plate', 0, 0, {size = 22})
    graphics2d.drawRect({0, plateY - 50, self.width, 100}, '#FF2A3550')
    graphics2d.drawRectOutline({0, plateY - 50, self.width, 100}, 3, '#FFFFD54F', {layer = 1})
    graphics2d.drawText(self.latin, Fitting.name, self.width / 2, plateY, self:nameStyle())

    local storyY = 240
    Test.caption('A story that wraps fits the height of its box', 0, storyY - 34, {size = 22})
    graphics2d.drawRect({0, storyY, self.width, self.height}, '#FF22303A')
    graphics2d.drawRectOutline({0, storyY, self.width, self.height}, 3, '#FF6FDCA0', {layer = 1})
    graphics2d.drawText(self.latin, Fitting.story, 0, storyY, self:storyStyle())

    local labelX = 720
    Test.caption('The same story without a smallest scale overflows its box', labelX, storyY - 34, {size = 22})
    graphics2d.drawRect({labelX, storyY, self.width, self.height}, '#FF22303A')
    graphics2d.drawRectOutline({labelX, storyY, self.width, self.height}, 3, '#FFFF8A84', {layer = 1})
    graphics2d.drawText(self.latin, Fitting.story, labelX, storyY, {size = 44, maxWidth = self.width})
end

return Fitting
