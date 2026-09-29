-- The particle images of the sample and the backdrop and labels the tests share.
local assets = require('haylen.assets')
local graphics2d = require('haylen.graphics2d')

local art = {}

function art.texture(name)
    return assets.texture('images/' .. name .. '.png', {filter = 'linear'})
end

-- Returns the source rectangles of an image made of square frames side by side.
function art.frames(texture)
    local frames = {}
    for index = 0, texture.width // texture.height - 1 do
        frames[#frames + 1] = {index * texture.height, 0, texture.height, texture.height}
    end
    return frames
end

-- Fills the view of the active world canvas with a night gradient, below everything else.
function art.backdrop(top, bottom)
    local area = graphics2d.canvasBounds()
    local bands = 12
    for index = 0, bands - 1 do
        local t = index / (bands - 1)
        local color = {top[1] + (bottom[1] - top[1]) * t, top[2] + (bottom[2] - top[2]) * t, top[3] + (bottom[3] - top[3]) * t, 1}
        graphics2d.drawRect({area.x, area.y + area.height * index / bands, area.width, area.height / bands + 1}, color, {layer = -10})
    end
end

-- Draws a caption under a point of the world, in a screen canvas.
function art.label(camera, text, x, y)
    local screenX, screenY = camera:worldToScreen(x, y)
    graphics2d.drawText(nil, text, screenX, screenY, {size = 30, anchor = {0.5, 0}, outlineWidth = 3})
end

return art
