-- Every object shape of Tiled and objects made from templates, outlined from `map:objects` and picked under the pointer, while `map:draw` draws the tile and text objects itself.
local assets = require('haylen.assets')
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local tiled = require('haylen.tiled')
local ui = require('haylen.ui')

local MapTest = require('categories.tiled.map-test')
local outlines = require('categories.tiled.outlines')

local Objects = haylen.class('Objects', MapTest)

local kColors = {rectangle = '#FF4FC3F7', ellipse = '#FF81C784', capsule = '#FFBA68C8', point = '#FFFFD54F', polygon = '#FFFF8A65', polyline = '#FFE57373', text = '#FFB0BEC5', tile = '#FFFFFFFF'}

function Objects:enter()
    self.map = tiled.newMapRenderer(assets.load('tiled/maps/objects.tmj'))
    local bounds = self.map.pixelBounds
    self:frame{
        hint = 'Point at an object to read it. The lamps and the signs come from templates, and the second lamp and the second sign override what their template says.',
        controls = {
            ui.checkbox{id = 'outlines', text = 'Outline every object', checked = true, onChange = function(event) self.showOutlines = event.checked end},
            ui.label{id = 'details', text = '', color = 'textMuted'},
        },
        view = {bounds.width + 64, bounds.height + 64},
        focus = 'outlines',
    }
    self.showOutlines = true
    self.objects = self.map:objects()
    self.camera:snapTo(bounds.width / 2, bounds.height / 2)
end

function Objects:update(dt)
    Objects.super.update(self, dt)
    self.hovered = nil
    for index = #self.objects, 1, -1 do
        local object = self.objects[index]
        if outlines.contains(self.map, object, self.pointer.worldX, self.pointer.worldY) then
            self.hovered = object
            break
        end
    end
    local object = self.hovered
    self:status(string.format('Objects %d, %s', #self.objects, object and string.format('pointing at "%s"', object.name) or 'point at one to read it'))
    if object == nil then
        self:details('Nothing under the pointer')
        return
    end
    local lines = {string.format('Object "%s" of shape "%s"', object.name, object.shape), string.format('Class %s', object.type ~= '' and '"' .. object.type .. '"' or 'none'), string.format('At %.0f, %.0f, size %.0f by %.0f', object.x, object.y, object.width, object.height), string.format('Rotation %.0f degrees', math.deg(object.rotation))}
    if object.template ~= '' then
        lines[#lines + 1] = 'Template "' .. object.template .. '"'
    end
    if object.shape == 'text' then
        lines[#lines + 1] = string.format('Text "%s", aligned "%s"', object.text.text, object.text.horizontalAlign)
    end
    for name, value in pairs(object.properties) do
        lines[#lines + 1] = string.format('Property "%s" is "%s"', name, tostring(value))
    end
    self:details(table.concat(lines, '\n'))
end

function Objects:draw(area)
    local map = self.map
    map:draw(self.camera)
    local unit = graphics2d.canvasUnitSize()
    if self.showOutlines then
        for _, object in ipairs(self.objects) do
            outlines.draw(map, object, kColors[object.shape], 3 * unit, {layer = 5})
        end
    end
    if self.hovered then
        outlines.draw(map, self.hovered, '#FFFFFFFF', 6 * unit, {layer = 6})
    end
end

return Objects
