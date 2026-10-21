-- Blend modes: the blend modes of a group over the same picture, one cell each, leaving out the premultiplied mode, which a group draws like alpha, and the opaque one, which replaces everything below the group with its image. The picture draws once into a render target that every cell shows as a shape with rounded corners. The modes up to screen use the blending of the GPU, and the modes from overlay on mix every color with the color below it, which the group reads from a copy of what its canvas drew before it.
local graphics = require('haylen.graphics')
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local ui = require('haylen.ui')

local Panels = require('categories.sprites.panels')
local SpriteTest = require('categories.sprites.sprite-test')
local Terrain = require('categories.sprites.terrain')

local BlendModes = haylen.class('BlendModes', SpriteTest)

BlendModes.code = [[
graphics2d.drawShape(cell, {radius = 14, texture = picture.texture})
graphics2d.beginGroup({blend = 'softLight', opacity = 0.9})  -- Mixes each color with the picture below.
graphics2d.drawCircle(x, y, 40, '#FFE5534B')
graphics2d.endGroup()]]

BlendModes.modes = {'alpha', 'additive', 'multiply', 'screen', 'overlay', 'darken', 'lighten', 'colorDodge', 'colorBurn', 'hardLight', 'softLight', 'difference', 'exclusion'}
BlendModes.titles = {alpha = 'Alpha', additive = 'Additive', multiply = 'Multiply', screen = 'Screen', overlay = 'Overlay', darken = 'Darken', lighten = 'Lighten', colorDodge = 'Color dodge', colorBurn = 'Color burn', hardLight = 'Hard light', softLight = 'Soft light', difference = 'Difference', exclusion = 'Exclusion'}
BlendModes.colors = {'#FFE5534B', '#FF35B37E', '#FF3C9BE0'}
BlendModes.gap = 14
BlendModes.radius = 14
-- The shape of a cell that the layout aims for, as its width over its height.
BlendModes.aspect = 1.35
-- The pixels of the picture for each unit of a cell, enough for screens of high density.
BlendModes.pictureScale = 2

function BlendModes:enter()
    self.tree = SpriteTest.texture('images/tree.png')
    self.hero = SpriteTest.texture('images/hero.png')
    local sky = graphics.newImage(4, 128)
    sky:fillGradient({angle = math.pi / 2, stops = {{0, '#FF4A90D9'}, {0.7, '#FF9ED3F0'}, {1, '#FFE8F4D8'}}})
    self.sky = graphics.newTexture(sky, {filter = 'linear'})
    self.pictureCamera = graphics2d.newCamera()
    self.opacity = 1
    self.time = 0
    self:frame{
        code = BlendModes.code,
        hint = 'Each cell draws the same picture and then a group of colored circles with its blend mode. Change the opacity of the groups to weaken every mode.',
        controls = {
            ui.formField{label = 'Opacity of the groups', ui.slider{id = 'opacity', min = 0, max = 1, value = self.opacity, step = 0.05, showValue = true, onChange = function(event) self.opacity = event.value end}},
        },
        focus = 'opacity',
    }
end

-- Lays the cells out in the number of rows whose cells come closest to the aimed shape, spreads the modes evenly over the rows and centers each row, and makes the picture again for the new size of a cell.
function BlendModes:resize(area)
    local count, gap = #BlendModes.modes, BlendModes.gap
    local best
    for rows = 1, count do
        local columns = math.ceil(count / rows)
        local width, height = (area.width - gap * (columns + 1)) / columns, (area.height - gap * (rows + 1)) / rows
        local miss = math.abs(math.log(width / height / BlendModes.aspect))
        if width > 0 and height > 0 and (not best or miss < best.miss) then
            best = {rows = rows, columns = columns, width = width, height = height, miss = miss}
        end
    end
    self.cells = {}
    local index = 1
    for row = 0, best.rows - 1 do
        local inRow = count // best.rows + (row < count % best.rows and 1 or 0)
        local left = (area.width - inRow * best.width - (inRow - 1) * gap) / 2
        for column = 0, inRow - 1 do
            self.cells[index] = {left + column * (best.width + gap), gap + row * (best.height + gap), best.width, best.height}
            index = index + 1
        end
    end
    local scale = BlendModes.pictureScale
    self.picture = graphics.newRenderTarget(math.ceil(best.width * scale), math.ceil(best.height * scale), {filter = 'linear'})
    self.pictureDirty = true
end

function BlendModes:update(dt)
    BlendModes.super.update(self, dt)
    self.time = self.time + dt
    local stats = graphics2d.stats()
    self:status(string.format('The %d blend modes draw through %d canvases in %d passes and %d draw calls.', #BlendModes.modes, stats.canvases, stats.passes, stats.drawCalls))
end

-- Draws the picture of every cell once: a sky over grass that turns to sand on the right, a tree and the hero.
function BlendModes:drawPicture()
    local target = self.picture
    local width, height = target.width, target.height
    self.pictureCamera.position = {width / 2, height / 2}
    graphics2d.beginTarget(target, self.pictureCamera, {clear = '#FF9ED3F0'})
    graphics2d.draw(self.sky, 0, 0, {width = width, height = height, pivotX = 0, pivotY = 0})
    local tile = height * 0.24
    local top = height * 0.58
    local columns, rows = math.ceil(width / tile), math.ceil((height - top) / tile)
    local grass, edges = Terrain.textures()
    local sandFrom = math.floor(columns * 0.62)
    for row = 0, rows - 1 do
        for column = 0, columns - 1 do
            Terrain.tiles(column, row, function(cornerColumn)
                return cornerColumn >= sandFrom and Terrain.sand or Terrain.grass
            end, function(texture, source)
                graphics2d.draw(texture == 1 and grass or edges, column * tile, top + row * tile, {source = source, width = tile, height = tile, pivotX = 0, pivotY = 0, layer = texture})
            end)
        end
    end
    local ground = top + tile * 0.4
    local tree, hero = height * 0.75 / self.tree.height, height * 0.46 / self.hero.height
    graphics2d.draw(self.tree, width * 0.24, ground, {scaleX = tree, scaleY = tree, pivotY = 1, layer = 3})
    graphics2d.draw(self.hero, width * 0.76, ground + tile * 0.25, {scaleX = hero, scaleY = hero, pivotY = 1, layer = 3})
end

-- The picture draws before the stage, which then shows it in every cell.
function BlendModes:render()
    if self.pictureDirty and self.picture then
        self.pictureDirty = false
        self:drawPicture()
    end
    BlendModes.super.render(self)
end

function BlendModes:draw(area)
    local texture = self.picture.texture
    for index, mode in ipairs(BlendModes.modes) do
        local cell = self.cells[index]
        local x, y, width, height = cell[1], cell[2], cell[3], cell[4]
        graphics2d.drawShape(cell, {radius = BlendModes.radius, texture = texture})
        local radius = math.min(width, height) * 0.2
        local centerX, centerY = x + width / 2, y + height * 0.58
        graphics2d.beginGroup({blend = mode, opacity = self.opacity, layer = 1})
        for circle, color in ipairs(BlendModes.colors) do
            local angle = self.time * 0.6 + circle * math.pi * 2 / 3
            graphics2d.drawCircle(centerX + math.cos(angle) * radius * 0.6, centerY + math.sin(angle) * radius * 0.6, radius, color)
        end
        graphics2d.endGroup()
        local title = BlendModes.titles[mode]
        local titleWidth = graphics2d.measureText(nil, title, {size = 24})
        Panels.pill(title, x + 12 + titleWidth / 2 + 14, y + 12 + 18, {size = 24, layer = 2})
    end
end

return BlendModes
