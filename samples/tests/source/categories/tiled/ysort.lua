-- Layers drawn with the `ysort` option in a canvas that sorts by y, so a hero walks behind and in front of trees, lamps and fences by the y its feet stand on.
local assets = require('haylen.assets')
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local input = require('haylen.input')
local ui = require('haylen.ui')

local MapTest = require('categories.tiled.map-test')

local YSort = haylen.class('YSort', MapTest)

YSort.code = [[
local hero = graphics2d.newSprite(texture, {pivotY = 1, width = 32, height = 48, layer = 1})  -- Sorts by its feet.
graphics2d.beginWorld(camera, {sort = 'y'})
map:drawLayer('ground', camera, {layer = -1})
map:drawLayer('props', camera, {layer = 1, ysort = true})
hero:draw()]]

local kSpeed = 220
local kArrive = 6
-- The hero starts with its feet a little above the trunk of a tree, so it starts half behind it.
local kStart = {574, 340}

function YSort:enter()
    self.map = MapTest.loadMap('tiled/maps/ysort.tmj')
    self.hero = graphics2d.newSprite(assets.texture('tiled/sprites/hero.png', {filter = 'linear'}), {pivotY = 1, width = 32, height = 48, layer = 1})
    local bounds = self.map.pixelBounds
    self:frame{
        hint = 'Walk with WASD, the arrows, the directional pad, the left stick or the touch stick, or tap or click where the hero should go. Switch y sorting off to see the hero drawn over everything.',
        code = YSort.code,
        controls = {
            ui.toggle{id = 'sort', text = 'Sort by y', checked = true, onChange = function(event) self.sorting = event.checked end},
            ui.checkbox{id = 'feet', text = 'Mark the sort lines', checked = true, onChange = function(event) self.showFeet = event.checked end},
        },
        view = {bounds.width * 0.55, bounds.height * 0.55},
        play = true,
        overlay = {ui.touchStick{action = 'move', radius = 110, mode = 'floating', touchOnly = true, anchor = 'bottomLeft', anchorTo = 'parent', margin = {0, 0, 24, 24}, width = 360, height = 300}},
    }
    self.sorting, self.showFeet = true, true
    self.position = {kStart[1], kStart[2]}
    self.camera:snapTo(kStart[1], kStart[2])
end

function YSort:exit()
    YSort.super.exit(self)
    input.clearVirtual()
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
        self.hero.flipHorizontal = x < 0
    end
    self.hero.x, self.hero.y = self.position[1], self.position[2]
    self:follow(dt)
    self:status(string.format('The feet of the hero stand at %.0f, %.0f, and the canvas sorts %s.', self.position[1], self.position[2], self.sorting and 'by y' or 'by layer only'))
end

-- The camera follows the hero and stops where the view would leave the map.
function YSort:follow(dt)
    local bounds, visible = self.map.pixelBounds, self.camera:visibleBounds()
    local halfWidth, halfHeight = visible.width / 2, visible.height / 2
    local x = math.max(halfWidth, math.min(bounds.width - halfWidth, self.position[1]))
    local y = math.max(halfHeight, math.min(bounds.height - halfHeight, self.position[2] - 24))
    local share = 1 - math.exp(-dt * 6)
    self.camera.x = self.camera.x + (x - self.camera.x) * share
    self.camera.y = self.camera.y + (y - self.camera.y) * share
end

-- The map draws in a canvas of its own that sorts by y, over the stage the frame drew.
function YSort:draw(area)
    graphics2d.beginWorld(self.camera, {sort = self.sorting and 'y' or 'layer'})
    local map, camera = self.map, self.camera
    map:drawLayer('ground', camera, {layer = -2})
    map:drawLayer('paths', camera, {layer = -1})
    map:drawLayer('fences', camera, {layer = 1, ysort = self.sorting})
    map:drawLayer('props', camera, {layer = 1, ysort = self.sorting})
    self.hero:draw()
    if self.showFeet then
        for _, object in ipairs(map:objects('props')) do
            graphics2d.drawShape({object.x - 18, object.y - 2, 36, 4}, {radius = 2, color = '#E6FFC93C', layer = 2})
        end
        graphics2d.drawShape({self.position[1] - 20, self.position[2] - 2.5, 40, 5}, {radius = 2.5, color = '#FF8FE3FF', layer = 2})
    end
end

return YSort
