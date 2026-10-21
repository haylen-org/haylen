-- The labels of the sprite tests that draw over pictures: a line of text centered on a dark rounded plate, so it reads on the sky, the grass or a sprite alike.
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')

local Plate = haylen.class('Plate')

Plate.size = 24
Plate.ink = '#FFF4F1E8'
Plate.color = '#E01A1D2C'
Plate.border = '#FF3A4160'
Plate.padding = {6, 14}

-- Draws `text` centered on `x`, `y`. The table `options` takes the `size` and `color` of the text, the `layer` of the plate, whose text draws one layer above, and an `accent` color for the border.
function Plate.draw(text, x, y, options)
    options = options or {}
    local size, layer = options.size or Plate.size, options.layer or 20
    local width, height = graphics2d.measureText(nil, text, {size = size})
    local padding = Plate.padding
    local plate = {x - width / 2 - padding[2], y - height / 2 - padding[1], width + padding[2] * 2, height + padding[1] * 2}
    graphics2d.drawShape(plate, {radius = plate[4] / 2, color = Plate.color, borderWidth = 2, borderColor = options.accent or Plate.border, layer = layer})
    graphics2d.drawText(nil, text, x, y, {size = size, color = options.color or Plate.ink, anchor = {0.5, 0.5}, layer = layer + 1})
end

return Plate
