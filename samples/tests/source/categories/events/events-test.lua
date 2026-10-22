-- The base of the events tests: they draw their listeners as painted icons on cards, with labels in the font of the art, and their logs in a card with the frame of every line. A test that sets `coinCounter` shows the coin counter of the player data autoload in the top-right corner of its play area.
local assets = require('haylen.assets')
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')

local Test = require('harness.test')

local EventsTest = haylen.class('EventsTest', Test)

-- The icons of `events/icons.png`, in cells of 128 pixels in reading order.
EventsTest.iconCell = 128
EventsTest.iconColumns = 4
EventsTest.iconNames = {'shield', 'heart', 'trophy', 'scroll', 'bolt', 'sparkle', 'bell', 'skull', 'book', 'hat', 'scarf', 'jukebox'}
EventsTest.card = '#FF232938'
EventsTest.cardBorder = '#FF2C3147'
EventsTest.lit = '#FFFFC93C'
EventsTest.coinCounter = false
-- The corner of the play area the coin counter takes, which a test that shows it keeps clear.
EventsTest.counterRoom = 96
-- The band at the top of the play area that holds the title of a test, under which its cards start.
EventsTest.titleRoom = 96
EventsTest.logSize = 24

function EventsTest:frame(options)
    self.font = assets.font('fonts/lilita-one-regular.ttf')
    self.icons = assets.texture('events/icons.png', {filter = 'linear'})
    self.iconSources = {}
    for index, name in ipairs(EventsTest.iconNames) do
        local cell = EventsTest.iconCell
        self.iconSources[name] = {(index - 1) % EventsTest.iconColumns * cell, (index - 1) // EventsTest.iconColumns * cell, cell, cell}
    end
    self.firstFrame = haylen.frameIndex()
    self.logStyle = {size = EventsTest.logSize, color = Test.ink, maxWidth = 0, layer = 2}
    self.frameStyle = {size = EventsTest.logSize, color = Test.muted, anchor = {1, 0}, layer = 2}
    EventsTest.super.frame(self, options)
    haylen.autoloads.playerData.counterShown = self.coinCounter
end

function EventsTest:exit()
    EventsTest.super.exit(self)
    haylen.autoloads.playerData.counterShown = false
end

-- The counter of the autoload follows the top-right corner of the play area wherever the layout puts it.
function EventsTest:update(dt)
    EventsTest.super.update(self, dt)
    if self.coinCounter and self.stage then
        haylen.autoloads.playerData.counterCorner = {self.stage.x + self.stage.width, self.stage.y}
    end
end

-- Draws the icon `name` centered at `x`, `y`, `size` units wide, with the optional `color` and `layer`.
function EventsTest:icon(name, x, y, size, color, layer)
    graphics2d.draw(self.icons, x, y, {source = self.iconSources[name], width = size, height = size, color = color, layer = layer or 2})
end

-- Draws a rounded card with a thin border, lit by a warm rim while `glow` is above 0.
function EventsTest.panel(rect, glow, layer)
    layer = layer or 0
    if glow and glow > 0 then
        graphics2d.drawShape({rect[1] - 6, rect[2] - 6, rect[3] + 12, rect[4] + 12}, {radius = 24, color = {1, 0.79, 0.24, 0.35 * glow}, softness = 18, layer = layer})
    end
    graphics2d.drawShape(rect, {radius = 18, color = EventsTest.card, borderWidth = glow and glow > 0 and 3 or 2, borderColor = glow and glow > 0 and {1, 0.79, 0.24, 0.4 + 0.6 * glow} or EventsTest.cardBorder, layer = layer})
end

-- Draws a short label in the font of the art, with the outline of the art, whose anchor sits at `x`, `y`.
function EventsTest:label(text, x, y, options)
    options = options or {}
    graphics2d.drawText(self.font, text, x, y, {size = options.size or 26, color = options.color or '#FFFFFFFF', anchor = options.anchor or {0.5, 0.5}, outlineWidth = options.outline or 3, outlineColor = '#FF15172A', maxWidth = options.maxWidth or 0, wrap = false, minScale = options.maxWidth and 0.6 or 1, layer = options.layer or 4})
end

-- Draws a pill with a short text centered at `x`, `y`, in the `fill` color, such as the priority of a listener.
function EventsTest:pill(text, x, y, fill, options)
    options = options or {}
    local size = options.size or 18
    local width = graphics2d.measureText(self.font, text, {size = size})
    local height = size * 1.5
    graphics2d.drawShape({x - width / 2 - size * 0.7, y - height / 2, width + size * 1.4, height}, {radius = height / 2, color = fill, layer = options.layer or 3})
    graphics2d.drawText(self.font, text, x, y, {size = size, color = options.color or '#FF15172A', anchor = {0.5, 0.5}, layer = (options.layer or 3) + 1})
end

-- Draws a journal in a card that fills `rect`, under a title, with the frame each line happened on, counted from the opening of the test, in a column of its own, or the empty state of `empty`, an icon name or `nil` and a line, while the journal has no lines. The newest lines that fit show with the newest at the bottom, and a line wider than the card wraps under its own start.
function EventsTest:logPanel(journal, rect, title, empty)
    EventsTest.panel(rect)
    local size = EventsTest.logSize
    local x, y = rect[1] + 20, rect[2] + 16
    local frameX, textX = x + size * 3.2, x + size * 4
    graphics2d.drawText(nil, 'Frame', frameX, y, {size = 20, color = Test.muted, anchor = {1, 0}, layer = 2})
    graphics2d.drawText(nil, title, textX, y, {size = 20, color = Test.muted, layer = 2})
    local body = {rect[1], y + 32, rect[3], rect[4] - 48}
    if journal.lines.size == 0 then
        if empty then
            self:emptyState(body, empty[1], empty[2])
        end
        return
    end

    -- The lines that fit are counted from the newest, and every line keeps a little room under it.
    local lines = journal.lines:values()
    local style = self.logStyle
    style.maxWidth = rect[1] + rect[3] - 20 - textX
    local gap = size * 0.25
    local used, first = 0, #lines + 1
    for index = #lines, 1, -1 do
        local _, height = graphics2d.measureText(nil, lines[index].text, style)
        if used + height > body[4] then
            break
        end
        used, first = used + height + gap, index
    end

    local top = body[2]
    for index = first, #lines do
        local line = lines[index]
        graphics2d.drawText(nil, tostring(line.frame - self.firstFrame), frameX, top, self.frameStyle)
        style.color = line.color
        graphics2d.drawText(nil, line.text, textX, top, style)
        local _, height = graphics2d.measureText(nil, line.text, style)
        top = top + height + gap
    end
end

-- Draws an empty state centered in `rect`: a faded icon over a line that says what to do, or a smaller icon beside the line when the room is too short for both, or the line alone without an icon. Nothing draws outside `rect`.
function EventsTest:emptyState(rect, icon, text)
    local x, y = rect[1] + rect[3] / 2, rect[2] + rect[4] / 2
    local style = {size = 26, color = Test.muted, maxWidth = rect[3] - 48, maxHeight = rect[4] - 16, minScale = 0.7, align = 'center', anchor = {0.5, 0.5}, layer = 2}
    local _, height = graphics2d.measureText(nil, text, style)
    local stacked = 96
    if not icon then
        graphics2d.drawText(nil, text, x, y, style)
        return
    end
    if stacked + 16 + height <= rect[4] - 16 then
        local top = y - (stacked + 16 + height) / 2
        self:icon(icon, x, top + stacked / 2, stacked, '#80FFFFFF')
        graphics2d.drawText(nil, text, x, top + stacked + 16 + height / 2, style)
        return
    end

    -- Beside the line, the icon takes the height the room has, and the line keeps the rest of the width.
    local size = math.min(64, rect[4] - 16)
    style.maxWidth = rect[3] - 48 - size - 16
    style.align = 'left'
    local width = graphics2d.measureText(nil, text, style)
    local left = x - (size + 16 + width) / 2
    self:icon(icon, left + size / 2, y, size, '#80FFFFFF')
    style.anchor = {0, 0.5}
    graphics2d.drawText(nil, text, left + size + 16, y, style)
end

return EventsTest
