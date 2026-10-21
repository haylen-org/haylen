-- The covers of the albums and mixes: SVG documents made from a style and the colors of a genre, read with graphics.newVectorImage and named for the UI with ui.setImage, so hundreds of covers cost a few hundred bytes each and rasterize only when they show.
local graphics = require('haylen.graphics')
local math2d = require('haylen.math')
local ui = require('haylen.ui')

local Catalog = require('data.catalog')
local Random = require('random')
local Style = require('style')

local Covers = {}

Covers.outline = '#1E1B33'

-- Turns a color of the catalog, '#AARRGGBB', into the '#RRGGBB' of SVG.
function Covers.svgColor(color)
    return '#' .. color:sub(4)
end

function Covers.document(spec)
    local random = Random(spec.seed)
    local background, second, accent = Covers.svgColor(spec.background), Covers.svgColor(spec.second), Covers.svgColor(spec.accent)
    local o = Covers.outline
    local shapes = {}
    local function add(text)
        shapes[#shapes + 1] = text
    end
    if spec.style == 1 then
        add(string.format('<circle cx="100" cy="92" r="52" fill="%s" stroke="%s" stroke-width="5"/>', accent, o))
        for line = 0, 4 do
            add(string.format('<rect x="0" y="%d" width="200" height="9" fill="%s"/>', 120 + line * 16, second))
        end
    elseif spec.style == 2 then
        for ring = 5, 1, -1 do
            add(string.format('<circle cx="100" cy="100" r="%d" fill="%s" stroke="%s" stroke-width="4"/>', ring * 17, ring % 2 == 0 and accent or second, o))
        end
    elseif spec.style == 3 then
        for band = -2, 4 do
            add(string.format('<path d="M%d 220 L%d -20 h26 L%d 220z" fill="%s" stroke="%s" stroke-width="4"/>', band * 46 - 40, band * 46 + 40, band * 46 - 14, band % 2 == 0 and accent or second, o))
        end
    elseif spec.style == 4 then
        for row = 0, 4 do
            for column = 0, 4 do
                local radius = 6 + random:integer(0, 10)
                add(string.format('<circle cx="%d" cy="%d" r="%d" fill="%s" stroke="%s" stroke-width="3"/>', 24 + column * 38, 24 + row * 38, radius, (row + column) % 3 == 0 and accent or second, o))
            end
        end
    elseif spec.style == 5 then
        for wave = 0, 5 do
            local y = 30 + wave * 30
            add(string.format('<path d="M-10 %d q27 -24 55 0 t55 0 t55 0 t55 0" fill="none" stroke="%s" stroke-width="12" stroke-linecap="round"/>', y, o))
            add(string.format('<path d="M-10 %d q27 -24 55 0 t55 0 t55 0 t55 0" fill="none" stroke="%s" stroke-width="6" stroke-linecap="round"/>', y, wave % 2 == 0 and accent or second))
        end
    else
        add(string.format('<circle cx="74" cy="84" r="50" fill="%s" stroke="%s" stroke-width="5" opacity=".95"/>', accent, o))
        add(string.format('<circle cx="128" cy="118" r="50" fill="%s" stroke="%s" stroke-width="5" opacity=".9"/>', second, o))
        add(string.format('<circle cx="98" cy="146" r="20" fill="#FFFFFF" stroke="%s" stroke-width="5"/>', o))
    end
    return string.format('<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 200 200" width="200" height="200">'
        .. '<defs><linearGradient id="sky" x1="0%%" y1="0%%" x2="100%%" y2="100%%"><stop offset="0" stop-color="%s"/><stop offset="1" stop-color="%s"/></linearGradient></defs>'
        .. '<rect width="200" height="200" fill="url(#sky)"/><g transform="rotate(%d 100 100)">%s</g>'
        .. '<path d="M0 0h200v60Q100 20 0 70z" fill="#FFFFFF" opacity=".12"/></svg>', background, second, spec.turn, table.concat(shapes))
end

-- Reads every cover of the catalog and names it for the UI.
function Covers.register()
    for name, spec in pairs(Catalog.coverSpecs) do
        ui.setImage(name, graphics.newVectorImage(Covers.document(spec)))
    end
end

-- Returns the backdrop of a genre, a tall texture that fades from a dark shade of its first color into the night of the app, made the first time a screen asks for it.
Covers.backdrops = {}

function Covers.backdrop(genreId)
    local texture = Covers.backdrops[genreId]
    if texture then
        return texture
    end
    local night = math2d.color(Style.color.night)
    local image = graphics.newImage(4, 256)
    image:fillGradient({angle = math.pi / 2, stops = {{0, Covers.deep(genreId, 0.55)}, {0.75, night}, {1, night}}})
    texture = graphics.newTexture(image, {filter = 'linear'})
    Covers.backdrops[genreId] = texture
    return texture
end

-- The first color of a genre mixed with the night of the app by `amount`, dark enough for white text.
function Covers.deep(genreId, amount)
    local color = math2d.color(Catalog.genre(genreId).colors[1])
    local night = math2d.color(Style.color.night)
    return math2d.color(color.r * amount + night.r * (1 - amount), color.g * amount + night.g * (1 - amount), color.b * amount + night.b * (1 - amount), 1)
end

-- The same color written as the color text that the style of a node takes.
function Covers.deepText(genreId, amount)
    local color = Covers.deep(genreId, amount)
    return string.format('#FF%02X%02X%02X', math.floor(color.r * 255 + 0.5), math.floor(color.g * 255 + 0.5), math.floor(color.b * 255 + 0.5))
end

return Covers
