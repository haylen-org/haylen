-- Tile animations of the tileset playing on tile layers and on tile objects as map:update advances the map time, with the frames of the tile under the pointer.
local haylen = require('haylen')
local assets = require('haylen.assets')
local graphics2d = require('haylen.graphics2d')
local input = require('haylen.input')
local tiled = require('haylen.tiled')
local ui = require('haylen.ui')

local sample = require('sample')

local Animations = haylen.class('Animations', sample.Test)

function Animations:enter()
    self.map = tiled.newMapRenderer(assets.load('maps/animations.tmj'))
    local bounds = self.map.bounds
    Animations.super.enter(self, {
        hint = 'Water, lava and torches animate on tile layers, and the coins and slimes are animated tile objects. Point at a tile to read its frames.',
        controls = {
            ui.toggle{id = 'play', text = 'Play', checked = true, onChange = function(event) self.playing = event.checked end},
            ui.label{text = 'Speed', font = 'caption', color = 'textMuted'},
            ui.slider{id = 'speed', value = 1, min = 0.25, max = 4, step = 0.25, showValue = true, onChange = function(event) self.speed = event.value end},
        },
        stats = true,
        view = {bounds.width + 64, bounds.height + 64},
        focus = 'play',
    })
    self.playing, self.speed, self.time = true, 1, 0
    self.camera:snapTo(bounds.width / 2, bounds.height / 2)
end

-- The frames of the tile under the pointer, looked up on the object layer first and then on the tile layers.
function Animations:describe(x, y)
    local map = self.map
    for _, object in ipairs(map:objects('animated')) do
        if x >= object.x and x < object.x + object.width and y >= object.y - object.height and y < object.y then
            return object.name, map:tileInfo(object.gid)
        end
    end
    local column, row = map:worldToCell(x, y)
    for _, layer in ipairs({'walls', 'ground'}) do
        local gid = map:tileAt(layer, column, row)
        if gid ~= 0 then
            return layer .. ' layer', map:tileInfo(gid)
        end
    end
end

function Animations:exit()
    Animations.super.exit(self)
    self.map = nil
end

function Animations:update(dt)
    Animations.super.update(self, dt)
    if input.pressed('reset') then
        self.speed = 1
        self:set('speed', {value = 1})
    end
    if self.playing then
        local step = dt * self.speed
        self.map:update(step)
        self.time = self.time + step
    end
    local where, info = self:describe(self.pointer.worldX, self.pointer.worldY)
    local frames = {}
    for _, frame in ipairs(info and info.animation or {}) do
        frames[#frames + 1] = string.format('  tile %d for %.0f ms', frame.tileId, frame.duration * 1000)
    end
    local summary = info and string.format('%s\nclass %s\n%s', where, info.type ~= '' and info.type or '-', #frames > 0 and 'frames\n' .. table.concat(frames, '\n') or 'still tile') or 'nothing under the pointer'
    self:showStats(string.format('map time %.1f s\n%s', self.time, summary))
end

function Animations:render()
    self:beginWorld()
    self.map:draw(self.camera)
end

return Animations
