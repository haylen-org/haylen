-- Textures with one pixel per cell, so a test draws a whole grid, a heat map or a generated map with one sprite.
local graphics = require('haylen.graphics')
local m = require('haylen.math')

local picture = {}

local kHeatSteps = 64
local kHeatStops = {m.color('#FF0D47A1'), m.color('#FF26A69A'), m.color('#FFFFEE58'), m.color('#FFEF6C00'), m.color('#FFB71C1C')}

-- The heat ramp is precomputed in steps, so heat maps reuse a few color strings.
local heatRamp = {}
for step = 0, kHeatSteps - 1 do
    local scaled = step / (kHeatSteps - 1) * (#kHeatStops - 1)
    local index = math.min(#kHeatStops - 2, math.floor(scaled))
    heatRamp[step] = kHeatStops[index + 1]:lerp(kHeatStops[index + 2], scaled - index):toHex()
end

-- Builds a texture where `color(column, row)` gives each pixel as a Color or a color string.
function picture.cells(width, height, color)
    local bytes = {}
    local char = string.char
    local cache = {}
    for row = 0, height - 1 do
        for column = 0, width - 1 do
            local value = color(column, row)
            local packed = cache[value]
            if packed == nil then
                local c = m.color(value)
                packed = char(math.floor(c.r * 255 + 0.5), math.floor(c.g * 255 + 0.5), math.floor(c.b * 255 + 0.5), math.floor(c.a * 255 + 0.5))
                cache[value] = packed
            end
            bytes[#bytes + 1] = packed
        end
    end
    return graphics.newTexture(width, height, {pixels = table.concat(bytes)})
end

-- Returns a heat color from blue at 0 through green and yellow to red at 1, as a color string.
function picture.heat(t)
    return heatRamp[math.floor(m.saturate(t) * (kHeatSteps - 1) + 0.5)]
end

return picture
