-- The colors of a picture: its palette by median cut, the swatches an interface takes from it, its placeholder text decoded into a blurred image, and text in the color that reads best on each swatch, with its contrast.
local assets = require('haylen.assets')
local graphics = require('haylen.graphics')
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local m = require('haylen.math')
local ui = require('haylen.ui')

local AlgorithmTest = require('categories.algorithms.algorithm-test')

local Colors = haylen.class('Colors', AlgorithmTest)

Colors.pictures = {{id = 'landscape', text = 'Landscape', path = 'interface/images/landscape.png'}, {id = 'planet', text = 'Planet', path = 'shaders/images/planet.png'}, {id = 'gems', text = 'Gems', path = 'sprites/atlases/gems.png'}}
Colors.swatchNames = {'dominant', 'vibrant', 'darkVibrant', 'lightVibrant', 'muted', 'darkMuted', 'lightMuted'}
Colors.box = {width = 620, height = 320}

function Colors:enter()
    self.picture, self.columns, self.rows, self.count = 'landscape', 4, 3, 8
    local items = {}
    for index, picture in ipairs(Colors.pictures) do
        items[index] = {id = picture.id, text = picture.text}
    end
    self:frame{
        hint = 'Pick a picture, and change how many cosines its placeholder keeps and how many colors its palette has. Each swatch writes its name in black or white, whichever reads better on it, with the contrast of the two.',
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
    self:status(string.format('Placeholder "%s", %d characters   Palette %d colors', self.code, #self.code, #self.palette))
end

function Colors:fit(x, y)
    local width = math.min(Colors.box.width, Colors.box.height * self.aspect)
    local height = width / self.aspect
    return {x + (Colors.box.width - width) / 2, y + (Colors.box.height - height) / 2, width, height}
end

function Colors:draw(area)
    local left, top = -790, -385
    local picture = self:fit(left, top)
    Colors.caption('The picture', left, top - 30)
    graphics2d.draw(self.texture, picture[1], picture[2], {width = picture[3], height = picture[4], pivotX = 0, pivotY = 0})
    local blurred = self:fit(left + 680, top)
    Colors.caption(string.format('Its placeholder of %d by %d cosines', self.columns, self.rows), left + 680, top - 30)
    graphics2d.draw(self.blurred, blurred[1], blurred[2], {width = blurred[3], height = blurred[4], pivotX = 0, pivotY = 0})

    -- The palette shows each color as wide as the share of the picture it stands for.
    local y = top + Colors.box.height + 55
    Colors.caption('Its palette, each color as wide as its share', left, y - 30)
    local x = left
    for _, swatch in ipairs(self.palette) do
        local width = swatch.population * 1300
        graphics2d.drawRect({x, y, width, 60}, swatch.color)
        x = x + width
    end

    y = y + 110
    Colors.caption('Its swatches, with text in the color that reads best and its contrast', left, y - 30)
    for index, name in ipairs(Colors.swatchNames) do
        local color = self.swatches[name]
        local cell = {left + (index - 1) * 186, y, 176, 150}
        if color then
            local text = color:contrastingColor()
            graphics2d.drawRect(cell, color)
            graphics2d.drawText(nil, name, cell[1] + 88, cell[2] + 52, {size = 24, color = text, anchor = {0.5, 0.5}})
            graphics2d.drawText(nil, string.format('%.1f to 1', m.contrast(text, color)), cell[1] + 88, cell[2] + 98, {size = 22, color = text, anchor = {0.5, 0.5}})
        else
            graphics2d.drawRectOutline(cell, 2, '#FF5A6A8A')
            graphics2d.drawText(nil, name .. '\nNone', cell[1] + 88, cell[2] + 75, {size = 22, color = '#FF8A9AB8', anchor = {0.5, 0.5}, align = 'center'})
        end
    end
end

return Colors
