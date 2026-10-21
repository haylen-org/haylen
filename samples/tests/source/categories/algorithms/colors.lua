-- The colors of a picture: its palette by median cut, the swatches an interface takes from it, its placeholder text decoded into a blurred image, and text in the color that reads best on each swatch, with its contrast.
local assets = require('haylen.assets')
local graphics = require('haylen.graphics')
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local m = require('haylen.math')
local ui = require('haylen.ui')

local AlgorithmTest = require('categories.algorithms.algorithm-test')
local Test = require('harness.test')

local Colors = haylen.class('Colors', AlgorithmTest)

Colors.code = [[
local image = assets.load('interface/images/landscape.png', 'image')
local palette = image:palette(8)  -- Colors and the share of the picture each stands for, by median cut.
local swatches = image:swatches()  -- The dominant, vibrant and muted colors an interface takes.
local code = image:placeholder({columns = 4, rows = 3})  -- A short text that holds a blurred image.
local blurred = graphics.decodePlaceholder(code, 32, 18)
local ink = swatches.vibrant:contrastingColor()  -- Black or white, whichever reads better on it.]]

Colors.pictures = {{id = 'landscape', text = 'Island', path = 'interface/images/landscape.png'}, {id = 'planet', text = 'Planet', path = 'shaders/images/planet.png'}, {id = 'gems', text = 'Gems', path = 'sprites/atlases/gems.png'}}
Colors.swatchNames = {'dominant', 'vibrant', 'darkVibrant', 'lightVibrant', 'muted', 'darkMuted', 'lightMuted'}
Colors.box = {width = 740, height = 380}
Colors.left = -770
Colors.gap = 60
Colors.width = 1540

function Colors:enter()
    self.picture, self.columns, self.rows, self.count = 'landscape', 4, 3, 8
    local items = {}
    for index, picture in ipairs(Colors.pictures) do
        items[index] = {id = picture.id, text = picture.text}
    end
    self:frame{
        hint = 'Pick a picture, and change how many cosines its placeholder keeps and how many colors its palette has. Each swatch writes its name in black or white, whichever reads better on it, with the contrast of the two.',
        code = Colors.code,
        controls = {
            ui.segmentedControl{id = 'picture', items = items, selected = self.picture, onChange = function(event)
                self.picture = event.value
                self:analyze()
            end},
            ui.formField{label = 'Cosines across', ui.slider{id = 'columns', min = 1, max = 9, step = 1, value = self.columns, showValue = true, decimals = 0, onChange = function(event)
                self.columns = event.value
                self:analyze()
            end}},
            ui.formField{label = 'Cosines down', ui.slider{id = 'rows', min = 1, max = 9, step = 1, value = self.rows, showValue = true, decimals = 0, onChange = function(event)
                self.rows = event.value
                self:analyze()
            end}},
            ui.formField{label = 'Palette colors', ui.slider{id = 'count', min = 1, max = 16, step = 1, value = self.count, showValue = true, decimals = 0, onChange = function(event)
                self.count = event.value
                self:analyze()
            end}},
        },
        focus = 'picture',
    }
    self:analyze()
end

-- Every analysis runs on the image in memory, so changing a slider shows its result at once.
function Colors:analyze()
    local path
    for _, picture in ipairs(Colors.pictures) do
        if picture.id == self.picture then
            path = picture.path
        end
    end
    local image = assets.load(path, 'image')
    self.texture = graphics.newTexture(image, {filter = 'linear'})
    self.aspect = image.width / image.height
    self.palette = image:palette(self.count)
    self.swatches = image:swatches()
    self.code = image:placeholder({columns = self.columns, rows = self.rows})
    self.blurred = graphics.newTexture(graphics.decodePlaceholder(self.code, 32, math.max(1, math.floor(32 / self.aspect))), {filter = 'linear'})
    self:status(string.format('The placeholder "%s" holds %d characters, and the palette %d colors', self.code, #self.code, #self.palette))
end

function Colors:fit(x, y)
    local width = math.min(Colors.box.width, Colors.box.height * self.aspect)
    local height = width / self.aspect
    return {x + (Colors.box.width - width) / 2, y + (Colors.box.height - height) / 2, width, height}
end

function Colors:draw(area)
    local left, top = Colors.left, -400
    local picture = self:fit(left, top + 34)
    Colors.caption('The picture', picture[1], top, {size = 24, color = Test.ink})
    graphics2d.drawShape(picture, {radius = 14, texture = self.texture})
    local blurred = self:fit(left + Colors.box.width + Colors.gap, top + 34)
    Colors.caption(string.format('Its placeholder of %d by %d cosines', self.columns, self.rows), blurred[1], top, {size = 24, color = Test.ink})
    graphics2d.drawShape(blurred, {radius = 14, texture = self.blurred})

    -- The palette shows each color as wide as the share of the picture it stands for, on a strip as wide as the swatches under it.
    local y = top + Colors.box.height + 92
    Colors.caption('Its palette, each color as wide as its share', left, y - 36, {size = 22})
    local x, total = left, 0
    for _, swatch in ipairs(self.palette) do
        total = total + swatch.population
    end
    for index, swatch in ipairs(self.palette) do
        local width = swatch.population / total * Colors.width
        local radius = {index == 1 and 12 or 0, index == #self.palette and 12 or 0, index == #self.palette and 12 or 0, index == 1 and 12 or 0}
        graphics2d.drawShape({x, y, width + (index < #self.palette and 0.5 or 0), 56}, {radius = radius, color = swatch.color})
        x = x + width
    end

    y = y + 112
    Colors.caption('Its swatches, with text in the color that reads best and its contrast', left, y - 36, {size = 22})
    local gap = 14
    local cellWidth = (Colors.width - gap * (#Colors.swatchNames - 1)) / #Colors.swatchNames
    for index, name in ipairs(Colors.swatchNames) do
        local color = self.swatches[name]
        local cell = {left + (index - 1) * (cellWidth + gap), y, cellWidth, 128}
        local middle = cell[1] + cellWidth / 2
        if color then
            local text = color:contrastingColor()
            graphics2d.drawShape(cell, {radius = 12, color = color})
            graphics2d.drawText(nil, name, middle, cell[2] + 44, {size = 24, color = text, anchor = {0.5, 0.5}, layer = 1})
            graphics2d.drawText(nil, string.format('%.1f to 1', m.contrast(text, color)), middle, cell[2] + 86, {size = 22, color = text, anchor = {0.5, 0.5}, layer = 1})
        else
            graphics2d.drawShape(cell, {radius = 12, color = '#00000000', borderWidth = 2, borderColor = '#FF5A6A8A'})
            graphics2d.drawText(nil, name .. '\nNone', middle, cell[2] + 64, {size = 22, color = '#FF8A9AB8', anchor = {0.5, 0.5}, align = 'center', layer = 1})
        end
    end
end

return Colors
