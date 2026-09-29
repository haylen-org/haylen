-- Layers drawn with the ysort option in a canvas that sorts by y, so a hero walks behind and in front of trees, lamps and fences by the y its feet stand on.
local haylen = require('haylen')
local assets = require('haylen.assets')
local graphics2d = require('haylen.graphics2d')
local input = require('haylen.input')
local tiled = require('haylen.tiled')
local ui = require('haylen.ui')

local sample = require('sample')

local YSort = haylen.class('YSort', sample.Test)

local kSpeed = 220
local kArrive = 6

function YSort:enter()
    self.map = tiled.newMapRenderer(assets.load('maps/ysort.tmj'))
    self.hero = graphics2d.newSprite(assets.texture('sprites/hero.png'), {pivotY = 1, layer = 1})
    local bounds = self.map.pixelBounds
    YSort.super.enter(self, {
        hint = 'Walk with WASD, the arrows, the left stick or the touch stick, or tap or click where the hero should go. Switch y sorting off to see the hero drawn over everything.',
        controls = {
            ui.toggle{id = 'sort', text = 'Sort by y', checked = true, onChange = function(event) self.sorting = event.checked end},
            ui.checkbox{id = 'feet', text = 'Mark the sort lines', onChange = function(event) self.showFeet = event.checked end},
        },
        stats = true,
        view = {bounds.width + 64, bounds.height + 64},
    })
    self.touch = ui.mount(ui.column{justify = 'end', padding = {0, 0, 110, 40}, onCancel = sample.back,
        ui.row{ui.touchStick{action = 'move', radius = 110, floating = true, touchOnly = true}},
    })
    self.sorting = true
    self.position = {bounds.width / 2, bounds.height / 2}
    self.camera:snapTo(bounds.width / 2, bounds.height / 2)
end

function YSort:exit()
    YSort.super.exit(self)
    self.touch:unmount()
    input.clearVirtual()
    self.map, self.hero = nil, nil
end

function YSort:update(dt)
    YSort.super.update(self, dt)
    local x, y = input.vector('move')
    if x ~= 0 or y ~= 0 then
        self.target = nil
    elseif self.pointer.pressed then
        self.target = {self.pointer.worldX, self.pointer.worldY}
    end
    if self.target then
        local dx, dy = self.target[1] - self.position[1], self.target[2] - self.position[2]
        local distance = math.sqrt(dx * dx + dy * dy)
        if distance < kArrive then
            self.target = nil
        else
            x, y = dx / distance, dy / distance
        end
    end
    local bounds = self.map.pixelBounds
    self.position[1] = math.max(16, math.min(bounds.width - 16, self.position[1] + x * kSpeed * dt))
    self.position[2] = math.max(48, math.min(bounds.height, self.position[2] + y * kSpeed * dt))
    if x ~= 0 then
        self.hero.flipX = x < 0
    end
    self.hero.x, self.hero.y = self.position[1], self.position[2]
    self:showStats(string.format('hero feet at %.0f, %.0f\nsorting %s\ndraws %d', self.position[1], self.position[2], self.sorting and 'by y' or 'by layer only', graphics2d.stats().sprites))
end

function YSort:render()
    self:beginWorld({sort = self.sorting and 'y' or 'layer'})
    local map, camera = self.map, self.camera
    map:drawLayer('ground', camera, {layer = 0})
    map:drawLayer('fences', camera, {layer = 1, ysort = self.sorting})
    map:drawLayer('props', camera, {layer = 1, ysort = self.sorting})
    self.hero:draw()
    if self.showFeet then
        for _, object in ipairs(map:objects('props')) do
            graphics2d.drawLine(object.x - 16, object.y, object.x + 16, object.y, 2, '#FFFFD54F', {layer = 2})
        end
        graphics2d.drawLine(self.position[1] - 16, self.position[2], self.position[1] + 16, self.position[2], 3, '#FF4FC3F7', {layer = 2})
    end
end

return YSort
