-- A Tiled world file: the castle it lists and the four islands its file name pattern finds, each map drawn at its place in the world through one camera.
local assets = require('haylen.assets')
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local input = require('haylen.input')
local m = require('haylen.math')
local tiled = require('haylen.tiled')
local ui = require('haylen.ui')

local MapTest = require('categories.tiled.map-test')
local Pan = require('categories.tiled.pan')

local Worlds = haylen.class('Worlds', MapTest)

local kMargin = 96

function Worlds:enter()
    self.placed = {}
    local area
    for _, entry in ipairs(assets.load('tiled/maps/world/overworld.world')) do
        local map = tiled.newMapRenderer(assets.load(entry.path))
        self.placed[#self.placed + 1] = {map = map, path = entry.path, bounds = m.rect(entry.x, entry.y, entry.width, entry.height), title = map.properties.title}
        area = area and area:merged(self.placed[#self.placed].bounds) or m.rect(entry.x, entry.y, entry.width, entry.height)
    end
    self.worldArea = area
    self:frame{
        hint = 'Drag, or use the arrows, the directional pad or the left stick, to pan, and the mouse wheel or the buttons to zoom. The map under the pointer is outlined with its path and its place in the world, and R or the X button shows the whole world.',
        controls = {
            ui.row{
                ui.button{id = 'in', text = 'Zoom in', onClick = function() self.pan:zoom(1.25) end},
                ui.button{id = 'out', text = 'Zoom out', onClick = function() self.pan:zoom(0.8) end},
            },
            ui.button{id = 'whole', text = 'Show the whole world', onClick = function() self:home() end},
        },
        view = {area.width + kMargin * 2, area.height + kMargin * 2},
        play = true,
    }
    self.pan = Pan(self)
    self:home()
end

function Worlds:home()
    self.zoomScale = 1
    local center = self.worldArea:center()
    self.camera:snapTo(center.x, center.y)
end

function Worlds:update(dt)
    Worlds.super.update(self, dt)
    if input.pressed('reset') then
        self:home()
    end
    self.pan:update(dt)
    self.hovered = nil
    for _, placed in ipairs(self.placed) do
        if placed.bounds:contains({self.pointer.worldX, self.pointer.worldY}) then
            self.hovered = placed
        end
    end
    local hovered = self.hovered
    self:status(string.format('Maps %d, world of %.0f by %.0f, %s', #self.placed, self.worldArea.width, self.worldArea.height, hovered and string.format('pointing at %s, file "%s" at %.0f, %.0f', hovered.title, hovered.path, hovered.bounds.x, hovered.bounds.y) or 'point at a map'))
end

function Worlds:draw(area)
    local unit = graphics2d.canvasUnitSize()
    for _, placed in ipairs(self.placed) do
        placed.map:draw(self.camera, {x = placed.bounds.x, y = placed.bounds.y})
        local color = placed == self.hovered and '#FFFFD54F' or '#66FFFFFF'
        graphics2d.drawRectOutline(placed.bounds, (placed == self.hovered and 5 or 2) * unit, color, {layer = 5})
        graphics2d.drawText(nil, placed.title, placed.bounds.x + 12, placed.bounds.y + 10, {size = 28 * unit, outlineWidth = 3 * unit, layer = 6})
    end
end

return Worlds
