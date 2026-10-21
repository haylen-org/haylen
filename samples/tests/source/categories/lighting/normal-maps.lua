-- Normal maps and specular: the textures and their normal maps are generated here from height fields, and the light height sets the angle the light reaches them at.
local graphics = require('haylen.graphics')
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local lighting2d = require('haylen.lighting2d')
local ui = require('haylen.ui')

local LightingTest = require('categories.lighting.lighting-test')
local Room = require('categories.lighting.room')

local NormalMaps = haylen.class('NormalMaps', LightingTest)

NormalMaps.code = [[
local lamp = lighting2d.newLight({radius = 700, color = '#FFFFEAD0', intensity = 1.6})
lamp.height = 90  -- Low lights graze the relief.
graphics2d.draw(bricks, x, y, {normalMap = brickNormals, specular = 1, shininess = 32})
graphics2d.drawLight(lamp)]]

NormalMaps.size = 128

-- Builds a color texture and its normal map from functions of a pixel. The height gives the relief, and the shine goes to the alpha of the normal map, which scales the specular highlight.
function NormalMaps.generate(height, color, shine)
    local size = NormalMaps.size
    local heights = {}
    for y = 0, size - 1 do
        for x = 0, size - 1 do
            heights[y * size + x] = height(x, y)
        end
    end

    local colors, normals = {}, {}
    for y = 0, size - 1 do
        for x = 0, size - 1 do
            -- Central differences of the height, with the y of the normal pointing up the image.
            local dx = heights[y * size + (x + 1) % size] - heights[y * size + (x - 1) % size]
            local dy = heights[((y + 1) % size) * size + x] - heights[((y - 1) % size) * size + x]
            local nx, ny, nz = -dx * 2, dy * 2, 1
            local length = math.sqrt(nx * nx + ny * ny + nz * nz)
            local index = y * size + x + 1
            local r, g, b = color(x, y, heights[y * size + x])
            colors[index] = string.char(r, g, b, 255)
            normals[index] = string.char(math.floor((nx / length * 0.5 + 0.5) * 255), math.floor((ny / length * 0.5 + 0.5) * 255), math.floor((nz / length * 0.5 + 0.5) * 255), shine(x, y))
        end
    end
    local options = {filter = 'linear', wrap = 'repeat'}
    options.pixels = table.concat(colors)
    local texture = graphics.newTexture(size, size, options)
    options.pixels = table.concat(normals)
    return texture, graphics.newTexture(size, size, options)
end

-- Returns a repeatable noise from 0 to 1 for a pixel and a seed, which roughens the bricks.
function NormalMaps.grain(x, y, seed)
    local value = math.sin(x * 12.9898 + y * 78.233 + seed * 37.719) * 43758.5453
    return value - math.floor(value)
end

-- Bricks of 64 by 32 pixels with bevelled, slightly uneven edges and grainy faces in warm clay colors, laid in light mortar, rough and barely shiny.
function NormalMaps.bricks()
    local function brick(x, y)
        local row = y // 32
        local offset = (row % 2) * 32
        return (x + offset) % 64, y % 32, ((x + offset) // 64) * 7 + row * 13
    end
    return NormalMaps.generate(function(x, y)
        local u, v, seed = brick(x, y)
        local edge = math.min(u, 63 - u, v, 31 - v)
        local bevel = edge < 2 and 0 or math.min(1, (edge - 1) / 5)
        return bevel * (0.9 + 0.1 * NormalMaps.grain(x // 3, y // 3, seed))
    end, function(x, y, level)
        local _, _, seed = brick(x, y)
        if level == 0 then
            local mortar = 168 + math.floor(NormalMaps.grain(x, y, 1) * 18)
            return mortar, mortar - 6, mortar - 16
        end
        local tone = seed * 37 % 5
        local grain = NormalMaps.grain(x, y, seed) * 20 - 10
        local red = 168 + tone * 9 + grain
        return math.floor(red), math.floor(red * 0.47 + tone * 3), math.floor(red * 0.32)
    end, function(x, y)
        return 40
    end)
end

-- A metal plate with round studs that shine.
function NormalMaps.studs()
    return NormalMaps.generate(function(x, y)
        local u, v = x % 32 - 15.5, y % 32 - 15.5
        local distance = math.sqrt(u * u + v * v)
        return distance < 11 and math.sqrt(121 - distance * distance) / 3 or 0
    end, function(x, y, level)
        return level > 0 and 200 or 110, level > 0 and 200 or 116, level > 0 and 210 or 128
    end, function(x, y)
        return 255
    end)
end

function NormalMaps:init(entry)
    NormalMaps.super.init(self, entry)
    self.ambientLight = '#FF1C1E26'
    self.brick, self.brickNormals = NormalMaps.bricks()
    self.plate, self.plateNormals = NormalMaps.studs()
    self.normalMapped = true
    self.specular = 1
    self.shininess = 32
    self.lamp = lighting2d.newLight({radius = 700, color = '#FFFFEAD0', intensity = 1.6, height = 90})
end

function NormalMaps:enter()
    self:frame{
        hint = 'Move the light with the mouse, a finger, the arrows, WASD or a stick. Low lights graze the surface and show the relief, high ones flatten it.',
        code = NormalMaps.code,
        cursor = true,
        controls = {
            ui.toggle{id = 'normalMaps', text = 'Normal maps', checked = true, onChange = function(event)
                self.normalMapped = event.checked
            end},
            ui.formField{label = 'Light height', ui.slider{id = 'height', min = 0, max = 400, value = self.lamp.height, showValue = true, decimals = 0, onChange = function(event)
                self.lamp.height = event.value
            end}},
            ui.formField{label = 'Specular', ui.slider{id = 'specular', min = 0, max = 3, value = self.specular, showValue = true, onChange = function(event)
                self.specular = event.value
            end}},
            ui.formField{label = 'Shininess', ui.slider{id = 'shininess', min = 1, max = 255, value = self.shininess, showValue = true, decimals = 0, onChange = function(event)
                self.shininess = event.value
            end}},
        },
    }
end

function NormalMaps:update(dt)
    NormalMaps.super.update(self, dt)
    self.lamp.x, self.lamp.y = self.cursorX, self.cursorY
    self:status(string.format('The light hangs %.0f units above the wall, with a specular strength of %.2f and a shininess of %.0f.', self.lamp.height, self.specular, self.shininess))
end

-- Tiles a texture over a rectangle, with its normal map when they are on.
function NormalMaps:drawSurface(texture, normals, rect)
    local tile = NormalMaps.size * 2
    for y = rect[2], rect[2] + rect[4] - tile, tile do
        for x = rect[1], rect[1] + rect[3] - tile, tile do
            graphics2d.draw(texture, x, y, {pivotX = 0, pivotY = 0, width = tile, height = tile, normalMap = self.normalMapped and normals or nil, specular = self.specular, shininess = self.shininess})
        end
    end
end

-- Bricks cover the left half of the canvas and the studded plate the right half, so the light starts on the seam between them.
function NormalMaps:draw(area)
    local canvas = graphics2d.canvasBounds()
    local tile = NormalMaps.size * 2
    local left = math.floor(canvas.x / tile) * tile
    local top = math.floor(canvas.y / tile) * tile
    local height = math.ceil((canvas:bottom() - top) / tile) * tile
    self:drawSurface(self.brick, self.brickNormals, {left, top, -left, height})
    self:drawSurface(self.plate, self.plateNormals, {0, top, math.ceil(canvas:right() / tile) * tile, height})
    graphics2d.drawLine(0, canvas.y, 0, canvas:bottom(), 8, '#FF1E1B33', {layer = 1})
    Room.drawLamp(self.lamp.x, self.lamp.y, self.lamp.color, 26)
    graphics2d.drawLight(self.lamp)
end

return NormalMaps
