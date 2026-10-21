-- Layer blend modes: a map built in code whose water layer lies over the ground with any of the thirteen blend modes of Tiled. The modes up to screen draw each tile with the blending of the GPU, and the modes from overlay on draw the layer into a group that mixes every color with the ground below it, the way Tiled composites the layer.
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local input = require('haylen.input')
local m = require('haylen.math')
local spatial2d = require('haylen.spatial2d')
local tiled = require('haylen.tiled')
local ui = require('haylen.ui')

local BuiltMap = require('categories.tiled.built-map')
local MapTest = require('categories.tiled.map-test')

local BlendLayers = haylen.class('BlendLayers', MapTest)

BlendLayers.code = [[
map:addLayer({name = 'water', blend = 'softLight'})  -- Or "mode": "soft-light" in the Tiled file.
map:fill('water', grid, firstWater)
map:draw(camera)  -- The layer draws into a group that mixes it with the ground below.]]

BlendLayers.modes = {'alpha', 'additive', 'multiply', 'screen', 'overlay', 'darken', 'lighten', 'colorDodge', 'colorBurn', 'hardLight', 'softLight', 'difference', 'exclusion'}
BlendLayers.tiledNames = {alpha = 'normal', additive = 'add', multiply = 'multiply', screen = 'screen', overlay = 'overlay', darken = 'darken', lighten = 'lighten', colorDodge = 'color-dodge', colorBurn = 'color-burn', hardLight = 'hard-light', softLight = 'soft-light', difference = 'difference', exclusion = 'exclusion'}
BlendLayers.titles = {alpha = 'Normal', additive = 'Add', multiply = 'Multiply', screen = 'Screen', overlay = 'Overlay', darken = 'Darken', lighten = 'Lighten', colorDodge = 'Color dodge', colorBurn = 'Color burn', hardLight = 'Hard light', softLight = 'Soft light', difference = 'Difference', exclusion = 'Exclusion'}

local kColumns, kRows, kTile = 40, 20, 32

function BlendLayers:enter()
    self.mode = 1
    self.map = tiled.newMapRenderer(tiled.newMap({width = kColumns, height = kRows, tileWidth = kTile, tileHeight = kTile}))
    self.terrain = self.map:addTileset(BuiltMap.painted('terrain'))
    self.shore = self.map:addTileset(BuiltMap.painted('shore'))
    self.map:addLayer({name = 'ground'})

    -- The ground is grass with flowers, and the water lies on the corners of the cells where a noise rises, so each cell takes the shore tile of its corners under water.
    local noise = m.noise(9)
    local ground, water = spatial2d.newCellGrid(kColumns, kRows), spatial2d.newCellGrid(kColumns, kRows)
    local function wet(column, row)
        return noise:fractal(column * 0.09 + 40, row * 0.09, 3, 2, 0.5) > 0.05 and 1 or 0
    end
    for row = 0, kRows - 1 do
        for column = 0, kColumns - 1 do
            ground:set(column, row, (column * 7 + row * 3) % 9 == 0 and 1 or 0)
            local mask = wet(column + 1, row) | wet(column + 1, row + 1) << 1 | wet(column, row + 1) << 2 | wet(column, row) << 3
            water:set(column, row, mask > 0 and mask or -1)
        end
    end
    self.map:fill('ground', ground, self.terrain)
    self.water = water
    self:applyMode()

    local buttons = {}
    for index, mode in ipairs(BlendLayers.modes) do
        buttons[index] = ui.button{id = mode, text = BlendLayers.titles[mode], variant = index == self.mode and 'primary' or 'default', onClick = function() self:pick(index) end}
    end
    self:frame{
        hint = 'Pick the blend mode of the water layer, or step through them with Q and E or the shoulder buttons.',
        code = BlendLayers.code,
        controls = {
            ui.label{text = 'Blend mode of the water layer', font = 'caption', color = 'textMuted'},
            ui.grid{columns = 2, gap = 10, alignItems = 'stretch', children = buttons},
        },
        view = {kColumns * kTile + 64, kRows * kTile + 64},
        focus = BlendLayers.modes[1],
    }
    self.camera:snapTo(kColumns * kTile / 2, kRows * kTile / 2)
end

-- A layer takes its blend mode when it is added, so the water layer comes back with the new mode.
function BlendLayers:applyMode()
    if self.hasWater then
        self.map:removeLayer('water')
    end
    self.hasWater = true
    self.map:addLayer({name = 'water', blend = BlendLayers.modes[self.mode]})
    self.map:fill('water', self.water, self.shore)
end

function BlendLayers:pick(index)
    self:set(BlendLayers.modes[self.mode], {variant = 'default'})
    self.mode = (index - 1) % #BlendLayers.modes + 1
    self:applyMode()
    self:set(BlendLayers.modes[self.mode], {variant = 'primary'})
    self:log('The water layer blends with the mode "%s".', BlendLayers.tiledNames[BlendLayers.modes[self.mode]])
end

function BlendLayers:update(dt)
    BlendLayers.super.update(self, dt)
    if input.pressed('next') then
        self:pick(self.mode + 1)
    elseif input.pressed('previous') then
        self:pick(self.mode - 1)
    end
    local stats = graphics2d.stats()
    self:status(string.format('The water layer blends with the mode "%s" of Tiled, in %d %s.', BlendLayers.tiledNames[BlendLayers.modes[self.mode]], stats.passes, stats.passes == 1 and 'pass' or 'passes'))
end

function BlendLayers:draw(area)
    self.map:draw(self.camera)
    MapTest.frameMap(self.map.pixelBounds)
end

return BlendLayers
