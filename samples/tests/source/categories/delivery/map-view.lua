-- Draws the small maps of the delivery tests, one character per tile: water, sand, grass and rock.
local graphics2d = require('haylen.graphics2d')
local m = require('haylen.math')

local mapView = {}

mapView.colors = {['~'] = '#FF2E5C8A', ['.'] = '#FFE8D29A', ['"'] = '#FF5FAF5A', ['^'] = '#FF8C8C99'}

-- Draws a map centered in the area with tiles as large as fit, or nothing for a map that is not loaded.
function mapView.draw(map, area)
    if not map then
        return
    end
    local rows = map.rows
    local columns = #rows[1]
    local tile = math.floor(math.min(area.width * 0.8 / columns, area.height * 0.8 / #rows))
    local left = (area.width - tile * columns) / 2
    local top = (area.height - tile * #rows) / 2
    for y, row in ipairs(rows) do
        for x = 1, columns do
            graphics2d.drawRect(m.rect(left + (x - 1) * tile, top + (y - 1) * tile, tile - 1, tile - 1), mapView.colors[row:sub(x, x)] or '#FFFF00FF')
        end
    end
end

return mapView
