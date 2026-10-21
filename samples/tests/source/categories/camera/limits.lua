-- Limits: the view stays inside a world rectangle while following. With limit smoothing it eases into the edge instead of stopping at it, and limits smaller than the view center it.
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local ui = require('haylen.ui')

local CameraTest = require('categories.camera.camera-test')
local Test = require('harness.test')
local Walker = require('categories.camera.walker')
local World = require('categories.camera.world')
local actions = require('categories.camera.actions')

local Limits = haylen.class('Limits', CameraTest)

Limits.code = [[
camera.limits = {-1800, -1000, 3600, 2000}
camera.positionSmoothing = true
camera.limitSmoothing = true  -- Eases into the edge instead of stopping at it.
camera:follow(walker.x, walker.y, dt)]]

Limits.sizes = {wide = {-1800, -1000, 3600, 2000}, narrow = {-1480, -620, 1000, 760}}
-- Until a player steers, the walker runs west along the road up to the left limit and back, so the view stops at it from the start.
Limits.route = {{-1720, 320, 1.4}, {-700, 200, 0.8}}

function Limits:init(entry)
    Limits.super.init(self, entry)
    self.world = World()
    self.walker = Walker(-900, 180, {action = 'move', route = Limits.route})
    self.camera = graphics2d.newCamera()
    self.camera.positionSmoothing = true
    self.camera.limitSmoothing = true
    self.size = 'wide'
    self.camera.limits = Limits.sizes.wide
    self.camera:snapTo(self.walker.x, self.walker.y)
end

function Limits:enter()
    self:loadActions(actions)
    self:frame{
        hint = 'Walk to an edge of the island with WASD, the arrows, the left stick or by holding the mouse button or a finger. The view stops at the yellow frame of the limits while the walker goes on.',
        code = Limits.code,
        play = true,
        controls = {
            ui.toggle{id = 'smoothing', text = 'Limit smoothing', checked = true, onChange = function(event)
                self.camera.limitSmoothing = event.checked
            end},
            ui.formField{label = 'Limits', ui.radioGroup{id = 'size', selected = 'wide', items = {{id = 'wide', text = 'Wider than the view'}, {id = 'narrow', text = 'Narrower, so the view centers'}}, onChange = function(event)
                self.size = event.value
                self.camera.limits = Limits.sizes[event.value]
            end}},
        },
    }
end

-- Names the limits the view rests against, as a sentence for the status line.
function Limits:against()
    local view, limits = self.camera:visibleBounds(), Limits.sizes[self.size]
    if view.width > limits[3] or view.height > limits[4] then
        return 'The limits are smaller than the view, so the view centers on them.'
    end
    local sides = {}
    if view.x <= limits[1] + 2 then
        sides[#sides + 1] = 'left'
    elseif view:right() >= limits[1] + limits[3] - 2 then
        sides[#sides + 1] = 'right'
    end
    if view.y <= limits[2] + 2 then
        sides[#sides + 1] = 'top'
    elseif view:bottom() >= limits[2] + limits[4] - 2 then
        sides[#sides + 1] = 'bottom'
    end
    if #sides == 0 then
        return 'The view follows the walker inside the limits.'
    end
    return string.format('The view rests against the %s limit while the walker goes on.', table.concat(sides, ' and '))
end

function Limits:update(dt)
    Limits.super.update(self, dt)
    self.walker:update(dt, self.world, self.camera)
    self.camera:follow(self.walker.x, self.walker.y, dt)
    self.camera:update(dt)
    self:status(self:against())
end

-- Shades the world outside the limits and frames them, so the edge the view stops at shows.
function Limits:draw(area)
    self.world:draw()
    self.walker:draw()
    local limits, bounds = Limits.sizes[self.size], World.bounds
    local right, bottom = limits[1] + limits[3], limits[2] + limits[4]
    local shade = '#70081018'
    graphics2d.drawRect({bounds.x - 2000, bounds.y - 2000, bounds.width + 4000, limits[2] - bounds.y + 2000}, shade, {layer = 9})
    graphics2d.drawRect({bounds.x - 2000, bottom, bounds.width + 4000, bounds:bottom() - bottom + 2000}, shade, {layer = 9})
    graphics2d.drawRect({bounds.x - 2000, limits[2], limits[1] - bounds.x + 2000, limits[4]}, shade, {layer = 9})
    graphics2d.drawRect({right, limits[2], bounds:right() - right + 2000, limits[4]}, shade, {layer = 9})
    graphics2d.drawShape(limits, {radius = 8, color = '#00000000', borderWidth = 8, borderColor = '#E0FFE08A', layer = 10})
    -- The label sits on a side of the frame that the view shows, near the top or the middle of the view.
    local view, unit = self.camera:visibleBounds(), graphics2d.canvasUnitSize()
    local middleX = math.max(limits[1], math.min(right, view.x + view.width / 2))
    local middleY = math.max(limits[2], math.min(bottom, view.y + view.height * 0.25))
    local slack = 4 * unit
    if limits[1] >= view.x - slack and limits[1] <= view:right() then
        CameraTest.tag('Limits', limits[1] + 24 * unit, middleY, {color = Test.warm, anchorX = 0})
    elseif right >= view.x and right <= view:right() + slack then
        CameraTest.tag('Limits', right - 24 * unit, middleY, {color = Test.warm, anchorX = 1})
    elseif limits[2] >= view.y - slack and limits[2] <= view:bottom() then
        CameraTest.tag('Limits', middleX, limits[2] + 70 * unit, {color = Test.warm})
    elseif bottom >= view.y and bottom <= view:bottom() + slack then
        CameraTest.tag('Limits', middleX, bottom - 20 * unit, {color = Test.warm})
    end
end

return Limits
