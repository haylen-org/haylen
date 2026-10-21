-- Every object shape of Tiled and objects made from templates, outlined from `map:objects` and picked under the pointer, while `map:draw` draws the tile and text objects itself.
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local ui = require('haylen.ui')

local MapTest = require('categories.tiled.map-test')
local outlines = require('categories.tiled.outlines')

local Objects = haylen.class('Objects', MapTest)

Objects.code = [[
for _, object in ipairs(map:objects()) do
    print(object.name, object.shape, object.type, object.x, object.y, object.rotation)
    print(object.template, #object.points)  -- The template path, and the points of polygons and polylines.
end
map:draw(camera)  -- Draws the tile and text objects itself.]]

local kColors = {rectangle = '#FF2F86F6', ellipse = '#FF2FB3C4', capsule = '#FFC9A0FF', point = '#FFFFC93C', polygon = '#FFF2884B', polyline = '#FFF0473E', text = '#FFF2F3FF', tile = '#FFF2F3FF'}

function Objects:enter()
    self.map = MapTest.loadMap('tiled/maps/objects.tmj')
    local bounds = self.map.pixelBounds
    self:frame{
        hint = 'Point at an object to read it. The lamps and the signs come from templates, and the second lamp and the second sign override what their template says.',
        code = Objects.code,
        controls = {
            ui.checkbox{id = 'outlines', text = 'Outline every object', checked = true, onChange = function(event) self.showOutlines = event.checked end},
            ui.label{id = 'details', text = '', color = 'textMuted'},
        },
        view = {bounds.width + 64, bounds.height + 64},
        focus = 'outlines',
    }
    self.showOutlines = true
    self.objects = self.map:objects()
    local counts, order = {}, {}
    for _, object in ipairs(self.objects) do
        if not counts[object.shape] then
            order[#order + 1] = object.shape
        end
        counts[object.shape] = (counts[object.shape] or 0) + 1
    end
    local lines = {string.format('The map holds %d objects:', #self.objects)}
    for _, shape in ipairs(order) do
        lines[#lines + 1] = string.format('%d of the shape "%s"', counts[shape], shape)
    end
    self.summary = table.concat(lines, '\n') .. '\n\nPoint at an object to read it.'
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
    self:status(object and string.format('The pointer is on the object "%s".', object.name) or 'Point at an object to read it.')
    if object == nil then
        self:details(self.summary)
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
    MapTest.frameMap(map.pixelBounds)
    local unit = graphics2d.canvasUnitSize()
    if self.showOutlines then
        for _, object in ipairs(self.objects) do
            if object.shape ~= 'text' and object.shape ~= 'tile' then
                outlines.draw(map, object, MapTest.ink, 7 * unit, {layer = 5})
                outlines.draw(map, object, kColors[object.shape], 4 * unit, {layer = 6})
            end
        end
    end
    -- The signs show the text of their template, or the text an instance overrides it with.
    for _, object in ipairs(self.objects) do
        if object.type == 'sign' then
            MapTest.pill(object.properties.text, object.x + object.width / 2, object.y - object.height - 4 * unit, {size = 18, layer = 7})
        end
    end
    if self.hovered then
        outlines.draw(map, self.hovered, MapTest.ink, 9 * unit, {layer = 7})
        outlines.draw(map, self.hovered, '#FFFFC93C', 5 * unit, {layer = 8})
    end
end

return Objects
