-- A Tiled world file: the castle it lists and the four islands its file name pattern finds, each map drawn at its place in the world through one camera.
local assets = require('haylen.assets')
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local input = require('haylen.input')
local m = require('haylen.math')
local ui = require('haylen.ui')

local MapTest = require('categories.tiled.map-test')
local Pan = require('categories.tiled.pan')

local Worlds = haylen.class('Worlds', MapTest)

Worlds.code = [[
for _, entry in ipairs(assets.load('tiled/maps/world/overworld.world')) do
    placed[#placed + 1] = {map = tiled.newMapRenderer(assets.load(entry.path)), x = entry.x, y = entry.y}
end
for _, part in ipairs(placed) do
    part.map:draw(camera, {x = part.x, y = part.y})  -- Maps the camera does not see are culled.
end]]

local kMargin = 96

function Worlds:enter()
    self.placed = {}
    local area
    for _, entry in ipairs(assets.load('tiled/maps/world/overworld.world')) do
        local map = MapTest.loadMap(entry.path)
        self.placed[#self.placed + 1] = {map = map, path = entry.path, bounds = m.rect(entry.x, entry.y, entry.width, entry.height), title = map.properties.title}
        area = area and area:merged(self.placed[#self.placed].bounds) or m.rect(entry.x, entry.y, entry.width, entry.height)
    end
    self.worldArea = area
    self.empty = self:gaps()
    self:frame{
        hint = 'Drag, or use the arrows, the directional pad or the left stick, to pan, and the mouse wheel or the buttons to zoom. The map under the pointer is outlined with its path and its place in the world, and R or the X button shows the whole world.',
        code = Worlds.code,
        controls = {
            ui.row{wrap = true,
                ui.button{id = 'in', text = 'Zoom in', onClick = function() self.pan:zoom(1.25) end},
                ui.button{id = 'out', text = 'Zoom out', onClick = function() self.pan:zoom(0.8) end},
            },
            ui.button{id = 'whole', text = 'Show the whole world', onClick = function() self:home() end},
            ui.sectionTitle{text = 'Maps of the world'},
            ui.list{id = 'maps', items = self:items(), focusable = false},
        },
        view = {area.width + kMargin * 2, area.height + kMargin * 2},
        play = true,
    }
    self.pan = Pan(self)
    self:home()
end

-- The maps with their files and their places in the world.
function Worlds:items()
    local items = {}
    for index, placed in ipairs(self.placed) do
        items[index] = {id = placed.path, text = placed.title, caption = string.format('"%s" at %.0f, %.0f', placed.path:match('[^/]+$'), placed.bounds.x, placed.bounds.y)}
    end
    return items
end

-- The places of the box of the world that no map covers, as rectangles of the size of a map.
function Worlds:gaps()
    local gaps = {}
    local size = self.placed[1].bounds
    for y = self.worldArea.y, self.worldArea.y + self.worldArea.height - 1, size.height do
        for x = self.worldArea.x, self.worldArea.x + self.worldArea.width - 1, size.width do
            local covered = false
            for _, placed in ipairs(self.placed) do
                covered = covered or placed.bounds:contains({x + 1, y + 1})
            end
            if not covered then
                gaps[#gaps + 1] = m.rect(x, y, size.width, size.height)
            end
        end
    end
    return gaps
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
    self:status(hovered and string.format('The pointer is on the %s, from "%s", placed at %.0f, %.0f.', hovered.title, hovered.path, hovered.bounds.x, hovered.bounds.y) or string.format('The world places %d maps over %.0f by %.0f units. Point at a map to read it.', #self.placed, self.worldArea.width, self.worldArea.height))
end

function Worlds:draw(area)
    local unit = graphics2d.canvasUnitSize()
    for _, gap in ipairs(self.empty) do
        graphics2d.drawShape({gap.x + 12, gap.y + 12, gap.width - 24, gap.height - 24}, {radius = 24, color = '#FF1C2130', borderWidth = 3 * unit, borderColor = '#FF3A4066'})
        MapTest.pill('Outside the world, where no map lies', gap.x + gap.width / 2, gap.y + gap.height / 2, {anchor = {0.5, 0.5}, size = 22})
    end
    for _, placed in ipairs(self.placed) do
        placed.map:draw(self.camera, {x = placed.bounds.x, y = placed.bounds.y})
        MapTest.pill(placed.title, placed.bounds.x + 14 * unit, placed.bounds.y + 14 * unit, {anchor = {0, 0}, size = 22})
    end
    MapTest.frameMap(self.worldArea)
    if self.hovered then
        graphics2d.drawShape(self.hovered.bounds, {color = '#00000000', borderWidth = 5 * unit, borderColor = '#FFFFC93C', layer = 9})
    end
end

return Worlds
