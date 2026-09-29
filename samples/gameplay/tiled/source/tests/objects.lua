-- Every object shape of Tiled and objects made from templates, outlined from map:objects and picked under the pointer, while map:draw draws the tile and text objects itself.
local haylen = require('haylen')
local assets = require('haylen.assets')
local graphics2d = require('haylen.graphics2d')
local tiled = require('haylen.tiled')
local ui = require('haylen.ui')

local outlines = require('outlines')
local sample = require('sample')

local Objects = haylen.class('Objects', sample.Test)

local kColors = {rectangle = '#FF4FC3F7', ellipse = '#FF81C784', capsule = '#FFBA68C8', point = '#FFFFD54F', polygon = '#FFFF8A65', polyline = '#FFE57373', text = '#FFB0BEC5', tile = '#FFFFFFFF'}

function Objects:enter()
    self.map = tiled.newMap(assets.load('maps/objects.tmj'))
    local bounds = self.map.bounds
    Objects.super.enter(self, {
        hint = 'Point at an object to read it. The lamps and the signs come from templates, and the second lamp and the second sign override what their template says.',
        controls = {
            ui.checkbox{id = 'outlines', text = 'Outline every object', checked = true, onChange = function(event) self.showOutlines = event.checked end},
        },
        stats = true,
        view = {bounds.width + 64, bounds.height + 64},
        focus = 'outlines',
    })
    self.showOutlines = true
    self.objects = self.map:objects()
    self.camera:snapTo(bounds.width / 2, bounds.height / 2)
end

function Objects:exit()
    Objects.super.exit(self)
    self.map, self.objects, self.hovered = nil, nil, nil
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
    if object == nil then
        self:showStats(string.format('objects %d\npoint at one to read it', #self.objects))
        return
    end
    local lines = {string.format('%s (%s)', object.name, object.shape), string.format('class %s', object.type ~= '' and object.type or '-'), string.format('at %.0f, %.0f size %.0f x %.0f', object.x, object.y, object.width, object.height), string.format('rotation %.0f degrees', math.deg(object.rotation))}
    if object.template ~= '' then
        lines[#lines + 1] = 'template ' .. object.template
    end
    if object.shape == 'text' then
        lines[#lines + 1] = string.format('text "%s", %s', object.text.text, object.text.horizontalAlign)
    end
    for name, value in pairs(object.properties) do
        lines[#lines + 1] = string.format('%s = %s', name, tostring(value))
    end
    self:showStats(table.concat(lines, '\n'))
end

function Objects:render()
    self:beginWorld()
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
