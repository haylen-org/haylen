-- The images and shaders of the sample, and the grid of captioned cells the tests show them in.
local assets = require('haylen.assets')
local graphics2d = require('haylen.graphics2d')

local art = {}

-- The pixel-art hero, 32 by 32 with a transparent border, drawn with nearest filtering.
function art.hero()
    return assets.texture('images/hero.png')
end

function art.planet()
    return assets.texture('images/planet.png', {filter = 'linear'})
end

function art.noise()
    return assets.texture('images/noise.png', {filter = 'linear', wrap = 'repeat'})
end

function art.pattern()
    return assets.texture('images/pattern.png', {filter = 'linear', wrap = 'repeat'})
end

function art.ramp(name)
    return assets.texture('images/ramp_' .. name .. '.png', {filter = 'linear'})
end

function art.material(name, uniforms)
    return graphics2d.newMaterial(assets.shader('shaders/' .. name .. '.shader'), uniforms)
end

-- Returns the centers of `count` cells in rows of `columns`, spread over the part of the screen below the header and left of the controls.
function art.cells(count, columns)
    local rows = math.ceil(count / columns)
    local width, height = 1340 / columns, 760 / rows
    local cells = {}
    for index = 1, count do
        local column, row = (index - 1) % columns, (index - 1) // columns
        cells[index] = {x = 60 + width * (column + 0.5), y = 270 + height * (row + 0.5), size = math.min(width, height) * 0.7}
    end
    return cells
end

function art.caption(text, x, y)
    graphics2d.drawText(nil, text, x, y, {size = 26, anchor = {0.5, 0}, outlineWidth = 3, layer = 10})
end

return art
