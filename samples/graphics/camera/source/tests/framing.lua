-- Framing several targets: camera:frame follows the middle of a list of points and zooms until they all fit with padding, within the zoom limits.
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local ui = require('haylen.ui')

local Player = require('player')
local World = require('world')
local sample = require('sample')

local Framing = haylen.class('Framing', sample.Test)

Framing.hints = 'Yellow walks with WASD or the left stick, blue with IJKL or the right stick, and red wanders by itself. Uncheck a player to leave it out of the frame.'

function Framing:init(entry)
    Framing.super.init(self, entry)
    self.world = World()
    self.players = {Player(-300, 0, '#FFFFD040', 'move'), Player(300, 100, '#FF40A0FF', 'move2'), Player(0, -300, '#FFFF5050')}
    self.framed = {true, true, true}
    self.camera = graphics2d.newCamera()
    self.camera.positionSmoothing = true
    self.camera.positionSmoothingSpeed = 3
    self.camera.minZoom = 0.3
    self.camera.maxZoom = 1.4
    self.time = 0
end

function Framing:controls()
    local names = {'Frame yellow', 'Frame blue', 'Frame red'}
    local checks = {}
    for index, name in ipairs(names) do
        checks[index] = ui.checkbox{text = name, checked = true, onChange = function(event)
            self.framed[index] = event.checked
        end}
    end
    return checks
end

function Framing:update(dt)
    self.time = self.time + dt
    self.players[1]:update(dt)
    self.players[2]:update(dt)
    local wanderer = self.players[3]
    wanderer.x, wanderer.y = math.cos(self.time * 0.4) * 1500, math.sin(self.time * 0.7) * 800

    local points = {}
    for index, player in ipairs(self.players) do
        if self.framed[index] then
            points[#points + 1] = {player.x, player.y}
        end
    end
    if #points > 0 then
        self.camera:frame(points, 220, dt)
    end
    self.camera:update(dt)
    self:setStatus(string.format('%d players framed, zoom %.2f', #points, self.camera.zoom.x))
end

function Framing:render()
    graphics2d.beginWorld(self.camera)
    self.world:draw()
    for _, player in ipairs(self.players) do
        player:draw()
    end
end

return Framing
