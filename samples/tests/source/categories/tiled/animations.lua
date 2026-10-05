-- Tile animations of the tileset playing on tile layers and on tile objects as `map:update` advances the map time, with the frames of the tile under the pointer.
local assets = require('haylen.assets')
local haylen = require('haylen')
local input = require('haylen.input')
local tiled = require('haylen.tiled')
local ui = require('haylen.ui')

local MapTest = require('categories.tiled.map-test')

local Animations = haylen.class('Animations', MapTest)

function Animations:enter()
    self.map = tiled.newMapRenderer(assets.load('tiled/maps/animations.tmj'))
    local bounds = self.map.pixelBounds
    self:frame{
        hint = 'Water, lava and torches animate on tile layers, and the coins and slimes are animated tile objects. Point at a tile to read its frames. R or the X button sets the speed back to 1.',
        controls = {
            ui.toggle{id = 'play', text = 'Play', checked = true, onChange = function(event) self.playing = event.checked end},
            ui.label{text = 'Speed', font = 'caption', color = 'textMuted'},
            ui.slider{id = 'speed', value = 1, min = 0.25, max = 4, step = 0.25, showValue = true, onChange = function(event) self.speed = event.value end},
            ui.label{text = 'Under the pointer', font = 'caption', color = 'textMuted'},
            ui.label{id = 'details', text = '', color = 'textMuted'},
        },
        view = {bounds.width + 64, bounds.height + 64},
        focus = 'play',
    }
    self.playing, self.speed, self.time = true, 1, 0
    self.camera:snapTo(bounds.width / 2, bounds.height / 2)
end

-- The frames of the tile under the pointer, looked up on the object layer first and then on the tile layers.
function Animations:describe(x, y)
    local map = self.map
    for _, object in ipairs(map:objects('animated')) do
        if x >= object.x and x < object.x + object.width and y >= object.y - object.height and y < object.y then
            return 'Object "' .. object.name .. '"', map:tileInfo(object.gid)
        end
    end
    local column, row = map:worldToCell(x, y)
    for _, layer in ipairs({'walls', 'ground'}) do
        local gid = map:tile(layer, column, row)
        if gid ~= 0 then
            return 'Layer "' .. layer .. '"', map:tileInfo(gid)
        end
    end
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
        frames[#frames + 1] = string.format('Tile %d for %.0f ms', frame.tileId, frame.duration * 1000)
    end
    local summary = info and string.format('%s\nClass %s\n%s', where, info.type ~= '' and '"' .. info.type .. '"' or 'none', #frames > 0 and table.concat(frames, '\n') or 'Still tile') or 'Nothing under the pointer'
    self:status(string.format('Map time %.1f s', self.time))
    self:details(summary)
end

function Animations:draw(area)
    self.map:draw(self.camera)
end

return Animations
