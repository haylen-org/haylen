-- Normal maps and specular: the textures and their normal maps are generated here from height fields, and the light height sets the angle the light reaches them at.
local graphics = require('haylen.graphics')
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local lighting2d = require('haylen.lighting2d')
local ui = require('haylen.ui')

local sample = require('sample')

local NormalMaps = haylen.class('NormalMaps', sample.Test)

NormalMaps.hints = 'Move the light with the mouse, a finger, WASD or the left stick. Low lights graze the surface and show the relief, high ones flatten it.'

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

-- Bricks of 64 by 32 pixels with bevelled edges, rough and barely shiny.
function NormalMaps.bricks()
    local function brick(x, y)
        local row = y // 32
        local offset = (row % 2) * 32
        return (x + offset) % 64, y % 32, ((x + offset) // 64) * 7 + row * 13
    end
    return NormalMaps.generate(function(x, y)
        local u, v = brick(x, y)
        local edge = math.min(u, 63 - u, v, 31 - v)
        return edge < 2 and 0 or math.min(1, (edge - 1) / 5)
    end, function(x, y, level)
        local _, _, seed = brick(x, y)
        if level == 0 then
            return 120, 116, 110
        end
        local shade = 150 + seed * 37 % 50
        return shade, math.floor(shade * 0.45), math.floor(shade * 0.3)
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
    self.camera = graphics2d.newCamera()
    self.cursor = sample.Cursor()
    self.brick, self.brickNormals = NormalMaps.bricks()
    self.plate, self.plateNormals = NormalMaps.studs()
    self.normalMapped = true
    self.specular = 1
    self.shininess = 32
    self.lamp = lighting2d.newLight({radius = 700, color = '#FFFFEAD0', intensity = 1.6, height = 90})
end

function NormalMaps:controls()
    return {
        ui.toggle{text = 'Normal maps', checked = true, onChange = function(event)
            self.normalMapped = event.checked
        end},
        ui.formField{label = 'Light height', ui.slider{min = 0, max = 400, value = self.lamp.height, showValue = true, decimals = 0, onChange = function(event)
            self.lamp.height = event.value
        end}},
        ui.formField{label = 'Specular', ui.slider{min = 0, max = 3, value = self.specular, showValue = true, onChange = function(event)
            self.specular = event.value
        end}},
        ui.formField{label = 'Shininess', ui.slider{min = 1, max = 255, value = self.shininess, showValue = true, decimals = 0, onChange = function(event)
            self.shininess = event.value
        end}},
    }
end

function NormalMaps:update(dt)
    self.cursor:update(dt)
    self.lamp.x, self.lamp.y = self.cursor:world(self.camera)
    self:setStatus(string.format('Height %.0f, specular %.2f, shininess %.0f', self.lamp.height, self.specular, self.shininess))
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

function NormalMaps:render()
    graphics2d.beginWorld(self.camera, {ambientLight = '#FF1C1E26'})
    self:drawSurface(self.brick, self.brickNormals, {-1024, -768, 1280, 1536})
    self:drawSurface(self.plate, self.plateNormals, {256, -768, 768, 1536})
    graphics2d.drawLight(self.lamp)
end

function NormalMaps:renderUi()
    graphics2d.beginScreen()
    self.cursor:draw()
end

return NormalMaps
