-- Framing several targets: `camera:frame` follows the middle of a list of points and zooms until they all fit with padding, within the zoom limits.
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local ui = require('haylen.ui')

local CameraTest = require('categories.camera.camera-test')
local Walker = require('categories.camera.walker')
local World = require('categories.camera.world')
local actions = require('categories.camera.actions')

local Framing = haylen.class('Framing', CameraTest)

Framing.code = [[
camera.positionSmoothing = true
camera.minZoom = 0.4
camera.maxZoom = 1.4
local points = {{yellow.x, yellow.y}, {blue.x, blue.y}, {red.x, red.y}}
camera:frame(points, 220, dt)  -- Fits every point with 220 units around.
camera:update(dt)]]

Framing.padding = 220
Framing.walkers = {
    {name = 'Yellow', color = '#FFFFD040', x = -500, y = 200, action = 'move'},
    {name = 'Blue', color = '#FF40A0FF', x = -200, y = 260, action = 'move2'},
    -- The red walker roams the road and the shore of the pond by itself.
    {name = 'Red', color = '#FFFF5050', x = -350, y = 230, route = {{1300, 580, 1}, {1900, 330}, {1500, -80, 0.8}, {300, 120}, {-1700, 330, 1}, {-350, 230}}},
}

function Framing:init(entry)
    Framing.super.init(self, entry)
    self.world = World()
    self.walkers, self.framed = {}, {}
    for index, spec in ipairs(Framing.walkers) do
        self.walkers[index] = Walker(spec.x, spec.y, {action = spec.action, route = spec.route, color = spec.color})
        self.framed[index] = true
    end
    self.camera = graphics2d.newCamera()
    self.camera.positionSmoothing = true
    self.camera.positionSmoothingSpeed = 3
    self.camera.minZoom = 0.4
    self.camera.maxZoom = 1.4
    self.camera:snapTo(-350, 230)
end

function Framing:enter()
    local checks = {}
    for index, spec in ipairs(Framing.walkers) do
        checks[index] = ui.checkbox{id = 'frame' .. index, text = 'Frame the ' .. spec.name:lower() .. ' walker', checked = true, onChange = function(event)
            self.framed[index] = event.checked
        end}
    end
    self:loadActions(actions)
    self:frame{
        hint = 'Yellow walks with WASD, the arrows or the left stick, blue with IJKL or the right stick, and red roams by itself. Uncheck a walker to leave it out of the frame.',
        code = Framing.code,
        play = true,
        controls = checks,
    }
end

-- Returns the points of the walkers in the frame.
function Framing:points()
    local points = {}
    for index, walker in ipairs(self.walkers) do
        if self.framed[index] then
            points[#points + 1] = {walker.x, walker.y}
        end
    end
    return points
end

function Framing:update(dt)
    Framing.super.update(self, dt)
    for _, walker in ipairs(self.walkers) do
        walker:update(dt, self.world, self.camera)
    end
    local points = self:points()
    if #points > 0 then
        self.camera:frame(points, Framing.padding, dt)
    end
    self.camera:update(dt)
    local zoom = math.floor(self.camera.zoom.x * 100 + 0.5)
    if #points == 0 then
        self:status('No walker is in the frame, so the view stays where it is.')
    else
        self:status(string.format('The view fits %d %s at %d percent.', #points, #points == 1 and 'walker' or 'walkers', zoom))
    end
end

function Framing:draw(area)
    self.world:draw()
    for _, walker in ipairs(self.walkers) do
        walker:draw()
    end
end

return Framing
