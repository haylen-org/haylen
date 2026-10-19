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

local kColumns, kRows, kTile = 40, 24, 32

function BlendLayers:enter()
    self.mode = 5
    self.map = tiled.newMapRenderer(tiled.newMap({width = kColumns, height = kRows, tileWidth = kTile, tileHeight = kTile}))
    self.terrain = self.map:addTileset(BuiltMap.painted('terrain'))
    self.shore = self.map:addTileset(BuiltMap.painted('shore'))
    self.map:addLayer({name = 'ground'})

    -- The ground is grass with flowers and paths, and the water covers the cells where a second noise rises.
    local noise = m.noise(9)
    local ground, water = spatial2d.newCellGrid(kColumns, kRows), spatial2d.newCellGrid(kColumns, kRows)
    for row = 0, kRows - 1 do
        for column = 0, kColumns - 1 do
            ground:set(column, row, noise:fractal(column * 0.12, row * 0.12, 3, 2, 0.5) > 0.3 and 2 or ((column * 7 + row * 3) % 9 == 0 and 1 or 0))
            water:set(column, row, noise:fractal(column * 0.07 + 40, row * 0.07, 3, 2, 0.5) > -0.05 and 15 or -1)
        end
    end
    self.map:fill('ground', ground, self.terrain)
    self.water = water
    self:applyMode()

    local items = {}
    for index, mode in ipairs(BlendLayers.modes) do
        items[index] = {id = mode, text = BlendLayers.tiledNames[mode]}
    end
    self:frame{
        hint = 'Pick the blend mode of the water layer, or step through them with Q and E or the shoulder buttons.',
        code = BlendLayers.code,
        controls = {
            ui.radioGroup{id = 'mode', items = items, selected = BlendLayers.modes[self.mode], onChange = function(event)
                for index, mode in ipairs(BlendLayers.modes) do
                    if mode == event.value then
                        self:pick(index)
                    end
                end
            end},
        },
        view = {kColumns * kTile + 64, kRows * kTile + 64},
        focus = 'mode',
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
    self.mode = (index - 1) % #BlendLayers.modes + 1
    self:applyMode()
    self:set('mode', {selected = BlendLayers.modes[self.mode]})
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
    self:status(string.format('Water layer "%s", passes %d, draw calls %d', BlendLayers.tiledNames[BlendLayers.modes[self.mode]], stats.passes, stats.drawCalls))
end

function BlendLayers:draw(area)
    self.map:draw(self.camera)
end

return BlendLayers
