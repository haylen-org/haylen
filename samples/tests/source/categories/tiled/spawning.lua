-- Entities made by `map:spawn` from the objects of a hidden layer, one factory per object class, placed with the offsets of their groups and drawn with the frames of their tiles.
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local input = require('haylen.input')
local ui = require('haylen.ui')

local MapTest = require('categories.tiled.map-test')

local Spawning = haylen.class('Spawning', MapTest)

Spawning.code = [[
local entities = map:spawn({
    coin = function(object) return {kind = 'coin', x = object.worldX, y = object.worldY, value = object.properties.value} end,
    chest = function(object) return {kind = 'chest', x = object.worldX, y = object.worldY, loot = object.properties.loot} end,
}, 'entities')  -- Objects of other classes, such as the marker, are skipped.]]

local kTouchDistance = 30

-- The global id of the flag tile of the terrain tileset, which marks the start.
Spawning.flagTile = 24

function Spawning:enter()
    self.map = MapTest.loadMap('tiled/maps/spawning.tmj')
    self.texture = self.map:tilesets()[1].texture
    self.flag = Spawning.flagTile
    local bounds = self.map.pixelBounds
    self:frame{
        hint = 'Tap or click entities: coins are collected, chests open and slimes get squashed. The layer of the objects is hidden, so only the spawned entities show, and the marker has no factory. R or the X button spawns them again.',
        code = Spawning.code,
        controls = {
            ui.button{id = 'respawn', text = 'Spawn again', onClick = function() self:populate() end},
            ui.sectionTitle{text = 'Factories by class'},
            ui.table{id = 'counts', columns = {{text = 'Class'}, {text = 'Becomes', width = 120}, {text = 'Now', width = 90, align = 'end'}}, rows = {}},
            ui.label{id = 'details', text = '', color = 'textMuted'},
        },
        view = {bounds.width + 64, bounds.height + 64},
        focus = 'respawn',
    }
    self.time = 0
    self.camera:snapTo(bounds.width / 2, bounds.height / 2)
    self:populate()
end

-- What each factory makes, by the class of the objects it takes.
Spawning.factories = {
    {class = 'player_start', kind = 'start', makes = 'Flag'},
    {class = 'coin', kind = 'coin', makes = 'Coin'},
    {class = 'slime', kind = 'slime', makes = 'Slime'},
    {class = 'chest', kind = 'chest', makes = 'Chest'},
    {class = 'sign', kind = 'sign', makes = 'Sign'},
}

-- Every factory turns the object table, with its world position, into an entity of the test.
function Spawning:populate()
    local function entity(kind, object, fields)
        fields.kind, fields.name, fields.gid = kind, object.name, object.gid
        fields.x, fields.y = object.worldX, object.worldY
        fields.width, fields.height = object.width, object.height
        return fields
    end
    self.entities = self.map:spawn({
        player_start = function(object) return entity('start', object, {}) end,
        coin = function(object) return entity('coin', object, {value = object.properties.value, phase = object.id}) end,
        slime = function(object) return entity('slime', object, {homeX = object.worldX, homeY = object.worldY, phase = object.id}) end,
        chest = function(object) return entity('chest', object, {loot = object.properties.loot}) end,
        sign = function(object) return entity('sign', object, {text = object.properties.text}) end,
    }, 'entities')
    self.skipped = #self.map:objects('entities') - #self.entities
    self.collected = 0
end

function Spawning:touch(x, y)
    for index, entity in ipairs(self.entities) do
        local cx, cy = entity.x + entity.width / 2, entity.y - entity.height / 2
        if entity.kind ~= 'sign' and entity.kind ~= 'start' and math.abs(cx - x) < kTouchDistance and math.abs(cy - y) < kTouchDistance then
            if entity.kind == 'coin' then
                self.collected = self.collected + entity.value
                table.remove(self.entities, index)
            elseif entity.kind == 'chest' then
                entity.open = true
            else
                entity.squashed = true
            end
            return
        end
    end
end

-- The source rectangle of a tile at this moment, following its animation from the phase of an entity when it has one.
function Spawning:source(gid, phase)
    local info = self.map:tileInfo(gid)
    local frames = info.animation
    if #frames == 0 then
        return info.source
    end
    local total = 0
    for _, frame in ipairs(frames) do
        total = total + frame.duration
    end
    local time = (self.time + (phase or 0) * 0.13) % total
    for _, frame in ipairs(frames) do
        if time < frame.duration then
            return self.map:tileInfo(gid - info.id + frame.tileId).source
        end
        time = time - frame.duration
    end
    return info.source
end

function Spawning:update(dt)
    Spawning.super.update(self, dt)
    if input.pressed('reset') then
        self:populate()
    end
    self.time = self.time + dt
    for _, entity in ipairs(self.entities) do
        if entity.kind == 'slime' and not entity.squashed then
            entity.x = entity.homeX + math.sin(self.time * 0.7 + entity.phase) * 40
            entity.y = entity.homeY - math.abs(math.sin(self.time * 3 + entity.phase)) * 10
        end
    end
    if self.pointer.pressed then
        self:touch(self.pointer.worldX, self.pointer.worldY)
    end
    local counts = {}
    for _, entity in ipairs(self.entities) do
        counts[entity.kind] = (counts[entity.kind] or 0) + 1
    end
    local rows = {}
    for _, factory in ipairs(Spawning.factories) do
        rows[#rows + 1] = {id = factory.class, cells = {factory.class, factory.makes, tostring(counts[factory.kind] or 0)}}
    end
    local key = table.concat({counts.coin or 0, counts.slime or 0, counts.chest or 0, self.collected}, ' ')
    if key ~= self.shownCounts then
        self.shownCounts = key
        self:set('counts', {rows = rows})
        self:details(string.format('%d object without a factory, the marker, was skipped.\nCoins collected are worth %d.', self.skipped, self.collected))
    end
    self:status(string.format('The factories made %d entities, and the coins collected are worth %d.', #self.entities, self.collected))
end

-- The start is a flag, and the other entities draw with the frames of their tiles, standing on their shadows.
function Spawning:draw(area)
    self.map:draw(self.camera)
    MapTest.frameMap(self.map.pixelBounds)
    for _, entity in ipairs(self.entities) do
        if entity.kind == 'start' then
            graphics2d.drawShape({entity.x - 20, entity.y - 7, 40, 14}, {kind = 'ellipse', color = '#40000000', layer = 4})
            graphics2d.draw(self.texture, entity.x, entity.y, {source = self.map:tileInfo(self.flag).source, width = 48, height = 48, pivotX = 0.3, pivotY = 0.95, layer = 5})
            MapTest.pill('Start', entity.x, entity.y - 52, {size = 16})
        else
            local bob = entity.kind == 'coin' and math.sin(self.time * 4 + entity.phase) * 4 or 0
            local height = entity.squashed and entity.height * 0.4 or entity.height
            graphics2d.drawShape({entity.x + entity.width * 0.15, entity.y - 6, entity.width * 0.7, 12}, {kind = 'ellipse', color = '#40000000', layer = 4})
            graphics2d.draw(self.texture, entity.x, entity.y + bob, {source = self:source(entity.gid, entity.phase), pivotX = 0, pivotY = 1, width = entity.width, height = height, layer = 5})
            if entity.kind == 'sign' then
                MapTest.pill(entity.text, entity.x + entity.width / 2, entity.y - entity.height - 4, {size = 16})
            elseif entity.open then
                MapTest.pill(entity.loot, entity.x + entity.width / 2, entity.y - entity.height - 4, {size = 16, color = '#FFFFC93C'})
            end
        end
    end
end

return Spawning
