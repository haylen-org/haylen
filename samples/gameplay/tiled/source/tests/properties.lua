-- Custom properties of every type read from the map, its layers, its objects and its tiles, shown as a tree with the property type names Tiled stores next to them.
local haylen = require('haylen')
local assets = require('haylen.assets')
local graphics2d = require('haylen.graphics2d')
local tiled = require('haylen.tiled')
local ui = require('haylen.ui')

local outlines = require('outlines')
local sample = require('sample')

local Properties = haylen.class('Properties', sample.Test)

local kTreeHeight = 560

-- Class values arrive as tables of their members and lists as sequences, so both open into children.
local function items(prefix, properties, types)
    local names = {}
    for name in pairs(properties) do
        names[#names + 1] = name
    end
    table.sort(names, function(a, b) return tostring(a) < tostring(b) end)
    local result = {}
    for _, name in ipairs(names) do
        local value = properties[name]
        local id = prefix .. '/' .. tostring(name)
        local kind = types and types[name] or type(value)
        if type(value) == 'table' then
            result[#result + 1] = {id = id, text = tostring(name), caption = kind == 'table' and (#value > 0 and 'list' or 'class') or kind, children = items(id, value)}
        else
            result[#result + 1] = {id = id, text = string.format('%s = %s', tostring(name), tostring(value)), caption = kind == 'userdata' and 'color' or kind}
        end
    end
    return result
end

function Properties:enter()
    self.map = tiled.newMap(assets.load('maps/properties.tmj'))
    local map = self.map
    local owners = {
        {id = 'map', text = 'Map of class ' .. map.type, properties = map.properties, types = map.propertyTypes},
        {id = 'ground', text = 'Layer ground', properties = map:layer('ground').properties, types = map:layer('ground').propertyTypes},
        {id = 'things', text = 'Layer things', properties = map:layer('things').properties, types = map:layer('things').propertyTypes},
    }
    self.objects = map:objects('things')
    for _, object in ipairs(self.objects) do
        owners[#owners + 1] = {id = 'object ' .. object.id, text = string.format('Object %s (id %d)', object.name, object.id), properties = object.properties, types = object.propertyTypes, object = object}
    end
    for _, gid in ipairs({self.objects[1].gid, map:tileAt('ground', 20, 5)}) do
        local info = map:tileInfo(gid)
        owners[#owners + 1] = {id = 'tile ' .. info.id, text = string.format('Tile %d of %s (%s)', info.id, info.tileset, info.type), properties = info.properties, types = info.propertyTypes}
    end

    local tree = {}
    self.owners = {}
    for _, owner in ipairs(owners) do
        tree[#tree + 1] = {id = owner.id, text = owner.text, children = items(owner.id, owner.properties, owner.types)}
        self.owners[owner.id] = owner
    end
    local bounds = map.bounds
    Properties.super.enter(self, {
        hint = 'Open the entries of the tree to read every value. Picking an object entry outlines the object on the map, and object properties hold the id of the object they point to.',
        controls = {
            ui.scroll{height = kTreeHeight, ui.tree{id = 'properties', items = tree, expanded = {'map', 'map/stats', 'map/loot'}, onSelect = function(event) self:select(event.item) end}},
        },
        view = {bounds.width + 64, bounds.height + 64},
        focus = 'properties',
    })
    self.camera:snapTo(bounds.width / 2, bounds.height / 2)
end

-- Picking an entry or one of its values outlines the object it belongs to.
function Properties:select(id)
    local owner = self.owners[id:match('^[^/]+')]
    self.selected = owner and owner.object
end

function Properties:exit()
    Properties.super.exit(self)
    self.map, self.objects, self.owners, self.selected = nil, nil, nil, nil
end

function Properties:render()
    self:beginWorld()
    local map = self.map
    map:draw(self.camera)
    local unit = graphics2d.canvasUnitSize()
    for _, object in ipairs(self.objects) do
        local selected = object == self.selected
        outlines.draw(map, object, selected and '#FFFFD54F' or '#AAFFFFFF', (selected and 6 or 3) * unit, {layer = 5})
        local top = object.shape == 'tile' and object.y - object.height or object.y
        graphics2d.drawText(nil, object.name, object.x, top - 8, {size = 22 * unit, anchor = {0, 1}, layer = 6, outlineWidth = 2 * unit})
    end
end

return Properties
