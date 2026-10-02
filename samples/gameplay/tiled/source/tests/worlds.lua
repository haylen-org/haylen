-- A Tiled world file: the castle it lists and the four islands its file name pattern finds, each map drawn at its place in the world through one camera.
local haylen = require('haylen')
local assets = require('haylen.assets')
local graphics2d = require('haylen.graphics2d')
local input = require('haylen.input')
local m = require('haylen.math')
local tiled = require('haylen.tiled')
local ui = require('haylen.ui')

local Pan = require('pan')
local sample = require('sample')

local Worlds = haylen.class('Worlds', sample.Test)

local kMargin = 96

function Worlds:enter()
    self.placed = {}
    local area
    for _, entry in ipairs(assets.load('maps/world/overworld.world')) do
        local map = tiled.newMapRenderer(assets.load(entry.path))
        self.placed[#self.placed + 1] = {map = map, path = entry.path, bounds = m.rect(entry.x, entry.y, entry.width, entry.height), title = map.properties.title}
        area = area and area:merged(self.placed[#self.placed].bounds) or m.rect(entry.x, entry.y, entry.width, entry.height)
    end
    self.area = area
    Worlds.super.enter(self, {
        hint = 'Drag to pan and use the mouse wheel or the buttons to zoom. The map under the pointer is outlined with its path and its place in the world.',
        controls = {
            ui.row{
                ui.button{id = 'in', text = 'Zoom in', onClick = function() self.pan:zoom(1.25) end},
                ui.button{id = 'out', text = 'Zoom out', onClick = function() self.pan:zoom(0.8) end},
            },
            ui.button{id = 'whole', text = 'Show the whole world', onClick = function() self:home() end},
        },
        stats = true,
        view = {area.width + kMargin * 2, area.height + kMargin * 2},
        focus = 'in',
    })
    self.pan = Pan.new(self)
    self:home()
end

function Worlds:home()
    self.zoomScale = 1
    local center = self.area:center()
    self.camera:snapTo(center.x, center.y)
end

function Worlds:exit()
    Worlds.super.exit(self)
    self.placed, self.hovered = nil, nil
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
    self:showStats(string.format('Maps %d\nWorld %.0f x %.0f\n%s', #self.placed, self.area.width, self.area.height, hovered and string.format('%s\nFile "%s"\nAt %.0f, %.0f', hovered.title, hovered.path, hovered.bounds.x, hovered.bounds.y) or 'Point at a map'))
end

function Worlds:render()
    self:beginWorld()
    local unit = graphics2d.canvasUnitSize()
    for _, placed in ipairs(self.placed) do
        placed.map:draw(self.camera, {x = placed.bounds.x, y = placed.bounds.y})
        local color = placed == self.hovered and '#FFFFD54F' or '#66FFFFFF'
        graphics2d.drawRectOutline(placed.bounds, (placed == self.hovered and 5 or 2) * unit, color, {layer = 5})
        graphics2d.drawText(nil, placed.title, placed.bounds.x + 12, placed.bounds.y + 10, {size = 28 * unit, outlineWidth = 3 * unit, layer = 6})
    end
end

return Worlds
