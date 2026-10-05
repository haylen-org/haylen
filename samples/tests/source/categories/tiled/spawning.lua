-- Entities made by `map:spawn` from the objects of a hidden layer, one factory per object class, placed with the offsets of their groups and drawn with the frames of their tiles.
local assets = require('haylen.assets')
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local input = require('haylen.input')
local tiled = require('haylen.tiled')
local ui = require('haylen.ui')

local MapTest = require('categories.tiled.map-test')

local Spawning = haylen.class('Spawning', MapTest)

local kTouchDistance = 28

function Spawning:enter()
    self.map = tiled.newMapRenderer(assets.load('tiled/maps/spawning.tmj'))
    self.texture = self.map:tilesets()[1].texture
    local bounds = self.map.pixelBounds
    self:frame{
        hint = 'Tap or click entities: coins are collected, chests open and slimes get squashed. The layer of the objects is hidden, so only the spawned entities show, and the marker has no factory. R or the X button spawns them again.',
        controls = {
            ui.button{id = 'respawn', text = 'Spawn again', onClick = function() self:populate() end},
        },
        view = {bounds.width + 64, bounds.height + 64},
        focus = 'respawn',
    }
    self.time = 0
    self.camera:snapTo(bounds.width / 2, bounds.height / 2)
    self:populate()
end

-- Every factory turns the object table, with its world position, into an entity of the test.
function Spawning:populate()
    local function entity(kind, object, fields)
        fields.kind, fields.name, fields.gid = kind, object.name, object.gid
        fields.x, fields.y = object.worldX, object.worldY
        return fields
    end
    self.entities = self.map:spawn({
        player_start = function(object) return entity('start', object, {}) end,
        coin = function(object) return entity('coin', object, {value = object.properties.value, phase = object.id}) end,
        slime = function(object) return entity('slime', object, {homeX = object.worldX, homeY = object.worldY, phase = object.id}) end,
        chest = function(object) return entity('chest', object, {loot = object.properties.loot}) end,
        sign = function(object) return entity('sign', object, {text = object.properties.text, width = object.width, height = object.height}) end,
    }, 'entities')
    self.skipped = #self.map:objects('entities') - #self.entities
    self.collected = 0
end

function Spawning:touch(x, y)
    for index, entity in ipairs(self.entities) do
        local cx, cy = entity.x + 16, entity.y - 16
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

-- The source rectangle of a tile at this moment, following its animation when it has one.
function Spawning:source(gid)
    local info = self.map:tileInfo(gid)
    local frames = info.animation
    if #frames == 0 then
        return info.source
    end
    local total = 0
    for _, frame in ipairs(frames) do
        total = total + frame.duration
    end
    local time = self.time % total
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
    self:status(string.format('Entities %d, coins %d, slimes %d, chests %d, signs %d, objects without a factory %d, collected %d', #self.entities, counts.coin or 0, counts.slime or 0, counts.chest or 0, counts.sign or 0, self.skipped, self.collected))
end

function Spawning:draw(area)
    self.map:draw(self.camera)
    for _, entity in ipairs(self.entities) do
        local order = {layer = 5}
        if entity.kind == 'sign' then
            graphics2d.drawRect({entity.x, entity.y, entity.width, entity.height}, '#FFA1887F', order)
            graphics2d.drawText(nil, entity.text, entity.x + entity.width / 2, entity.y - 6, {size = 20, anchor = {0.5, 1}, outlineWidth = 2, layer = 6})
        elseif entity.kind == 'start' then
            graphics2d.drawRing(entity.x, entity.y, 20, 4, '#FF66BB6A', order)
            graphics2d.drawText(nil, 'Start', entity.x, entity.y - 26, {size = 20, anchor = {0.5, 1}, outlineWidth = 2, layer = 6})
        else
            local bob = entity.kind == 'coin' and math.sin(self.time * 4 + entity.phase) * 4 or 0
            local height = entity.squashed and 12 or 32
            graphics2d.draw(self.texture, entity.x, entity.y + bob, {source = self:source(entity.gid), pivotX = 0, pivotY = 1, width = 32, height = height, layer = 5})
            if entity.open then
                graphics2d.drawText(nil, entity.loot, entity.x + 16, entity.y - 40, {size = 20, anchor = {0.5, 1}, color = '#FFFFD54F', outlineWidth = 2, layer = 6})
            end
        end
    end
end

return Spawning
