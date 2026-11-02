-- Custom properties of every type read from the map, its layers, its objects and its tiles, shown as a tree with the property type names Tiled stores next to them.
local assets = require('haylen.assets')
local graphics = require('haylen.graphics')
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local ui = require('haylen.ui')

local MapTest = require('categories.tiled.map-test')
local outlines = require('categories.tiled.outlines')

local Properties = haylen.class('Properties', MapTest)

Properties.code = [[
print(map.properties.level, map.properties.tint, map.properties.stats.spawn.x)  -- An integer, a Color and a class member.
print(map.properties.loot[6][1], map.propertyTypes.weather)  -- A list inside a list, and the name of an enum type.
print(map:layer('ground').properties.season)
local guard = map:objects('things')[2]
print(guard.properties.watches, guard.properties.route[2])  -- An object id, and an item of a list.]]

local kGuardSize = 72

-- The names of the swatches made for the color properties, which the test gives back when it exits.
Properties.swatches = {}

-- Returns the name of a round swatch of a color, made once, which a tree item shows as its picture.
function Properties.swatch(color)
    local hex = color:toHex()
    local name = '@property-color-' .. hex:sub(2)
    if not Properties.swatches[name] then
        Properties.swatches[name] = true
        local svg = string.format('<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24"><circle cx="12" cy="12" r="10" fill="#%s" fill-opacity="%.3f" stroke="#1E1B33" stroke-width="2"/></svg>', hex:sub(4), color.a)
        ui.setImage(name, graphics.newVectorImage(svg))
    end
    return name
end

-- Class values arrive as tables of their members and lists as sequences, so both open into children, and a color shows its swatch.
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
            local color = type(value) == 'userdata' and value.toHex
            result[#result + 1] = {id = id, text = string.format('%s = %s', tostring(name), tostring(value)), caption = color and 'color' or kind, image = color and Properties.swatch(value) or nil, iconColor = color and false or nil}
        end
    end
    return result
end

function Properties:enter()
    self.map = MapTest.loadMap('tiled/maps/properties.tmj')
    local map = self.map
    local owners = {
        {id = 'map', text = string.format('The map, of the class "%s"', map.type), properties = map.properties, types = map.propertyTypes},
        {id = 'ground', text = 'The layer "ground"', properties = map:layer('ground').properties, types = map:layer('ground').propertyTypes},
        {id = 'things', text = 'The layer "things"', properties = map:layer('things').properties, types = map:layer('things').propertyTypes},
    }
    self.objects = map:objects('things')
    for _, object in ipairs(self.objects) do
        owners[#owners + 1] = {id = 'object ' .. object.id, text = string.format('The object "%s", id %d', object.name, object.id), properties = object.properties, types = object.propertyTypes, object = object}
    end
    for _, gid in ipairs({self.objects[1].gid, map:tile('water', 18, 4)}) do
        local info = map:tileInfo(gid)
        owners[#owners + 1] = {id = 'tile ' .. info.id, text = string.format('The tile %d of "%s", a %s', info.id, info.tileset, info.type), properties = info.properties, types = info.propertyTypes}
    end

    local tree = {}
    self.owners = {}
    for _, owner in ipairs(owners) do
        tree[#tree + 1] = {id = owner.id, text = owner.text, children = items(owner.id, owner.properties, owner.types)}
        self.owners[owner.id] = owner
    end
    local bounds = map.pixelBounds
    self:frame{
        hint = 'Open the entries of the tree to read every value. Picking an entry of an object outlines it on the map, and object properties hold the id of the object they point to.',
        code = Properties.code,
        controls = {
            ui.tree{id = 'properties', items = tree, expanded = {'map', 'map/stats', 'map/loot'}, onSelect = function(event) self:select(event.item) end},
        },
        view = {bounds.width + 64, bounds.height + 64},
        focus = 'properties',
    }
    self.guard = assets.texture('sprites/sheets/walker.png', {filter = 'linear'})
    self.camera:snapTo(bounds.width / 2, bounds.height / 2)
end

-- Picking an entry or one of its values outlines the object it belongs to.
function Properties:select(id)
    local owner = self.owners[id:match('^[^/]+')]
    self.selected = owner and owner.object
end

function Properties:exit()
    for name in pairs(Properties.swatches) do
        ui.removeImage(name)
    end
    Properties.swatches = {}
    Properties.super.exit(self)
end

-- The haunted zone draws as the drifting mist of the color its property names, the guard as a character standing on its point, and the treasure as the chest of its tile, each named in a pill.
function Properties:draw(area)
    local map = self.map
    map:draw(self.camera)
    MapTest.frameMap(map.pixelBounds)
    local unit = graphics2d.canvasUnitSize()
    for _, object in ipairs(self.objects) do
        local selected = object == self.selected
        local top = object.y
        if object.type == 'zone' then
            self:drawMist(object)
        elseif object.type == 'npc' then
            graphics2d.drawShape({object.x - 22, object.y - 8, 44, 14}, {kind = 'ellipse', color = '#40000000', layer = 4})
            graphics2d.draw(self.guard, object.x, object.y, {source = {0, 0, 256, 256}, width = kGuardSize, height = kGuardSize, pivotY = 0.92, flipHorizontal = true, layer = 5})
            top = object.y - kGuardSize * 0.85
        elseif object.shape == 'tile' then
            top = object.y - object.height
        end
        if selected then
            outlines.draw(map, object, MapTest.ink, 9 * unit, {layer = 6})
            outlines.draw(map, object, '#FFFFC93C', 5 * unit, {layer = 7})
        end
        MapTest.pill(object.name, object.x + (object.shape == 'point' and 0 or object.width / 2), top - 6 * unit, {size = 18, layer = 8})
    end
end

-- Soft puffs of the fog color drift slowly over the zone, thinner toward its edges, so the zone reads as mist over the trees.
function Properties:drawMist(zone)
    local time = haylen.elapsed()
    local fog = zone.properties.fog
    local radius = math.min(zone.width, zone.height) * 0.42
    for index = 0, 6 do
        local share = index / 6
        local x = zone.x + zone.width * (0.15 + share * 0.7) + math.sin(time * 0.4 + index * 1.7) * radius * 0.25
        local y = zone.y + zone.height * (0.5 + 0.22 * math.sin(index * 2.3)) + math.cos(time * 0.3 + index) * radius * 0.15
        local size = radius * (0.8 + 0.25 * math.sin(index * 1.3))
        graphics2d.drawShape({x - size, y - size * 0.7, size * 2, size * 1.4}, {kind = 'ellipse', color = fog:withAlpha(fog.a * 0.55), softness = size * 0.8, layer = 4})
    end
end

return Properties
