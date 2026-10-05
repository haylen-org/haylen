-- Framing several targets: `camera:frame` follows the middle of a list of points and zooms until they all fit with padding, within the zoom limits.
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local ui = require('haylen.ui')

local Test = require('harness.test')
local Walker = require('categories.camera.walker')
local World = require('categories.camera.world')
local actions = require('categories.camera.actions')

local Framing = haylen.class('Framing', Test)

Framing.names = {'Frame yellow', 'Frame blue', 'Frame red'}

function Framing:init(entry)
    Framing.super.init(self, entry)
    self.world = World()
    self.walkers = {Walker(-300, 0, '#FFFFD040', 'move'), Walker(300, 100, '#FF40A0FF', 'move2'), Walker(0, -300, '#FFFF5050')}
    self.framed = {true, true, true}
    self.camera = graphics2d.newCamera()
    self.camera.positionSmoothing = true
    self.camera.positionSmoothingSpeed = 3
    self.camera.minZoom = 0.3
    self.camera.maxZoom = 1.4
    self.time = 0
end

function Framing:enter()
    local checks = {}
    for index, name in ipairs(Framing.names) do
        checks[index] = ui.checkbox{id = 'frame' .. index, text = name, checked = true, onChange = function(event)
            self.framed[index] = event.checked
        end}
    end
    self:loadActions(actions)
    self:frame{
        hint = 'Yellow walks with WASD, the arrows or the left stick, blue with IJKL or the right stick, and red wanders by itself. Uncheck a walker to leave it out of the frame.',
        play = true,
        controls = checks,
    }
end

function Framing:update(dt)
    Framing.super.update(self, dt)
    self.time = self.time + dt
    self.walkers[1]:update(dt)
    self.walkers[2]:update(dt)
    local wanderer = self.walkers[3]
    wanderer.x, wanderer.y = math.cos(self.time * 0.4) * 1500, math.sin(self.time * 0.7) * 800

    local points = {}
    for index, walker in ipairs(self.walkers) do
        if self.framed[index] then
            points[#points + 1] = {walker.x, walker.y}
        end
    end
    if #points > 0 then
        self.camera:frame(points, 220, dt)
    end
    self.camera:update(dt)
    self:status(string.format('Walkers framed %d   Zoom %.2f', #points, self.camera.zoom.x))
end

function Framing:draw(area)
    self.world:draw()
    for _, walker in ipairs(self.walkers) do
        walker:draw()
    end
end

return Framing
