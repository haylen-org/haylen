-- The base of the events tests: while one of them shows, the player data autoload draws its coin counter in the top-right corner of the play area, and the tests draw their listeners as painted icons on cards, with labels in the font of the art.
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
-- The corner of the play area the coin counter takes, which the tests keep clear.
EventsTest.counterRoom = 96

function EventsTest:frame(options)
    self.font = assets.font('fonts/lilita-one-regular.ttf')
    self.icons = assets.texture('events/icons.png', {filter = 'linear'})
    self.iconSources = {}
    for index, name in ipairs(EventsTest.iconNames) do
        local cell = EventsTest.iconCell
        self.iconSources[name] = {(index - 1) % EventsTest.iconColumns * cell, (index - 1) // EventsTest.iconColumns * cell, cell, cell}
    end
    EventsTest.super.frame(self, options)
    haylen.autoloads.playerData.counterShown = true
end

function EventsTest:exit()
    EventsTest.super.exit(self)
    haylen.autoloads.playerData.counterShown = false
end

-- The counter of the autoload follows the top-right corner of the play area wherever the layout puts it.
function EventsTest:update(dt)
    EventsTest.super.update(self, dt)
    if self.stage then
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

-- Draws a journal in a card that fills `rect`, under a title, with the frame each line happened on in a column of its own, or the empty state of `empty`, an icon and a line, while the journal has no lines.
function EventsTest:logPanel(journal, rect, title, empty)
    EventsTest.panel(rect)
    local size = 24
    local x, y = rect[1] + 20, rect[2] + 16
    graphics2d.drawText(nil, 'Frame', x + size * 3.2, y, {size = 20, color = Test.muted, anchor = {1, 0}, layer = 2})
    graphics2d.drawText(nil, title, x + size * 4, y, {size = 20, color = Test.muted, layer = 2})
    local body = {rect[1], y + 32, rect[3], rect[4] - 48}
    if journal.lines.size == 0 and empty then
        self:emptyState(body, empty[1], empty[2])
        return
    end
    journal:draw(x, body[2], body[4], size, rect[3] - 40)
end

-- Draws an empty state centered in `rect`: a faded icon and a line that says what to do.
function EventsTest:emptyState(rect, icon, text)
    local x, y = rect[1] + rect[3] / 2, rect[2] + rect[4] / 2
    self:icon(icon, x, y - 40, 96, '#80FFFFFF')
    graphics2d.drawText(nil, text, x, y + 28, {size = 26, color = Test.muted, anchor = {0.5, 0}, maxWidth = rect[3] - 48, align = 'center', layer = 2})
end

return EventsTest
