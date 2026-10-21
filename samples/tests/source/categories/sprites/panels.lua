-- The panels that sprite tests lay their demonstrations out in: a landscape or a portrait layout of rounded cards with a title, captions centered under what they name and labels on dark pills that read over any picture.
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')

local Test = require('harness.test')

local Panels = haylen.class('Panels')

Panels.fill = '#FF1F2434'
Panels.border = '#FF2C3147'
Panels.radius = 18
Panels.padding = 20
Panels.titleSize = 30
Panels.captionSize = 28
Panels.pillColor = '#D8141826'

-- Returns the layout of `layouts`, a landscape and a portrait one with the `view` each lays its cards in, that the camera shows larger in the play area of `area`.
function Panels.layout(layouts, area)
    local landscape, portrait = layouts.landscape, layouts.portrait
    local wide = math.min(area.width / landscape.view[1], area.height / landscape.view[2])
    local tall = math.min(area.width / portrait.view[1], area.height / portrait.view[2])
    return wide >= tall and landscape or portrait
end

-- Draws a card with its title in the top-left corner and returns the area under the title, inside the padding.
function Panels.card(rect, title, options)
    options = options or {}
    local x, y, width, height = rect[1], rect[2], rect[3], rect[4]
    local layer = options.layer or -10
    graphics2d.drawShape(rect, {radius = Panels.radius, color = options.fill or Panels.fill, borderWidth = 2, borderColor = Panels.border, layer = layer})
    local padding = Panels.padding
    if not title then
        return {x + padding, y + padding, width - padding * 2, height - padding * 2}
    end
    graphics2d.drawText(nil, title, x + padding, y + padding - 4, {size = Panels.titleSize, color = Test.ink, layer = layer + 1})
    local top = Panels.titleSize * 1.2 + 10
    return {x + padding, y + padding + top, width - padding * 2, height - padding * 2 - top}
end

-- Draws a caption whose top center sits at `x`, `y`.
function Panels.caption(text, x, y, options)
    options = options or {}
    graphics2d.drawText(nil, text, x, y, {size = options.size or Panels.captionSize, color = options.color or Test.ink, anchor = options.anchor or {0.5, 0}, layer = options.layer or 5})
end

-- Draws a label on a dark pill centered at `x`, `y`, for names drawn over a picture.
function Panels.pill(text, x, y, options)
    options = options or {}
    local size = options.size or 24
    local width, height = graphics2d.measureText(nil, text, {size = size})
    local layer = options.layer or 20
    graphics2d.drawShape({x - width / 2 - size * 0.6, y - height / 2 - size * 0.25, width + size * 1.2, height + size * 0.5}, {radius = height, color = options.fill or Panels.pillColor, layer = layer})
    graphics2d.drawText(nil, text, x, y, {size = size, color = options.color or '#FFFFFFFF', anchor = {0.5, 0.5}, layer = layer + 1})
end

return Panels
