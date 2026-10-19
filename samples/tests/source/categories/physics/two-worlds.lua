-- Two worlds: two physics worlds side by side, the left one pulling down and the right one pulling up, and a body that crosses the border between them moves to the other world with `body:moveToWorld`, keeping its speed, spin, material and data, and obeys that world from then on. A cannon throws bodies across, the pointer throws more, and the sliders change the gravity of each world.
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local physics2d = require('haylen.physics2d')
local ui = require('haylen.ui')

local Grab = require('categories.physics.grab')
local parts = require('categories.physics.parts')
local PhysicsTest = require('categories.physics.physics-test')

local TwoWorlds = haylen.class('TwoWorlds', PhysicsTest)

TwoWorlds.code = [[
local land = physics2d.newWorld({gravity = {0, 980}})
local sky = physics2d.newWorld({gravity = {0, -600}})
if body.world == land and body.x > 0 then
    body = body:moveToWorld(sky)  -- Keeps its speed, spin, shapes and data, and falls up from now on.
end]]

TwoWorlds.colors = {'#FF3C9BE0', '#FFE8A93A', '#FF35B37E', '#FFE5534B', '#FFA87FE0'}

function TwoWorlds:enter()
    self.gravity = {left = 980, right = -600}
    self:build()
    self:frame{
        code = TwoWorlds.code,
        hint = 'Click or tap anywhere to throw a body from the cannon toward that point, or drag a body across the border. R or the X button starts over.',
        controls = {
            ui.button{id = 'throw', text = 'Throw five bodies', variant = 'primary', onClick = function() self:volley() end},
            ui.formField{label = 'Gravity of the left world', ui.slider{id = 'left', min = -1500, max = 1500, step = 50, value = 980, showValue = true, decimals = 0, onChange = function(event)
                self.gravity.left = event.value
                self.left.gravity = {0, event.value}
            end}},
            ui.formField{label = 'Gravity of the right world', ui.slider{id = 'right', min = -1500, max = 1500, step = 50, value = -600, showValue = true, decimals = 0, onChange = function(event)
                self.gravity.right = event.value
                self.right.gravity = {0, event.value}
            end}},
            ui.button{id = 'reset', text = 'Start over', onClick = function() self:build() end},
        },
    }
end

-- Builds two worlds that share the play area: each has a floor at the end its gravity pulls toward and walls at its outer side, and the border between them is open.
function TwoWorlds:build()
    self.left = physics2d.newWorld({gravity = {0, self.gravity.left}})
    self.right = physics2d.newWorld({gravity = {0, self.gravity.right}})
    self.grabs = {left = Grab(self.left), right = Grab(self.right)}
    self.statics = {}
    for _, world in ipairs({self.left, self.right}) do
        for _, wall in ipairs({{0, 420, 1600, 40}, {0, -420, 1600, 40}}) do
            local body = world:createBody({type = 'static', x = wall[1], y = wall[2]})
            parts.box(body, wall[3], wall[4])
            self.statics[#self.statics + 1] = body
        end
    end
    local leftWall = self.left:createBody({type = 'static', x = -790, y = 0})
    parts.box(leftWall, 20, 800)
    local rightWall = self.right:createBody({type = 'static', x = 790, y = 0})
    parts.box(rightWall, 20, 800)
    self.statics[#self.statics + 1] = leftWall
    self.statics[#self.statics + 1] = rightWall
    self.bodies = {}
    self.crossings = 0
    self.thrown = 0
end

-- Throws a body from the cannon in the left world toward a point, with a speed that grows with the distance.
function TwoWorlds:throw(x, y)
    local fromX, fromY = -700, 300
    local dx, dy = x - fromX, y - fromY
    local length = math.max(1, math.sqrt(dx * dx + dy * dy))
    local speed = math.min(1400, 500 + length)
    self.thrown = self.thrown + 1
    local body = self.left:createBody({x = fromX, y = fromY, vx = dx / length * speed, vy = dy / length * speed, angularVelocity = 3})
    local color = TwoWorlds.colors[(self.thrown - 1) % #TwoWorlds.colors + 1]
    if self.thrown % 2 == 0 then
        parts.circle(body, 22, {restitution = 0.5})
    else
        parts.box(body, 44, 44, {restitution = 0.3})
    end
    parts.paint(body, color)
    body.data.number = self.thrown
    self.bodies[#self.bodies + 1] = body
    if #self.bodies > 40 then
        table.remove(self.bodies, 1):destroy()
    end
end

function TwoWorlds:volley()
    for index = 1, 5 do
        self:throw(-100 + index * 120, -260 + index * 40)
    end
end

-- Moves every body whose center crossed the border to the world on that side. The outlines that `parts` paints live in the data of the body, so the moved body takes them along.
function TwoWorlds:cross()
    for index, body in ipairs(self.bodies) do
        local target = (body.world == self.left and body.x > 0 and self.right) or (body.world == self.right and body.x < 0 and self.left)
        if target then
            local moved = body:moveToWorld(target)
            self.bodies[index] = moved
            self.crossings = self.crossings + 1
            self:log('Body %d moved to the %s world at %.0f units per second.', moved.data.number, target == self.right and 'right' or 'left', moved.velocity:length())
        end
    end
end

function TwoWorlds:update(dt)
    TwoWorlds.super.update(self, dt)
    if not self.stage then
        return
    end
    local pointer = self.pointer
    if pointer.pressed and not self.grabs.left:holding() and not self.grabs.right:holding() then
        local x, y = pointer.worldX, pointer.worldY
        if #self.left:queryPoint(x, y) == 0 and #self.right:queryPoint(x, y) == 0 then
            self:throw(x, y)
        end
    end
    self.grabs.left:update(pointer, self.camera)
    self.grabs.right:update(pointer, self.camera)

    self:simulate(self.left, dt)
    self:simulate(self.right, dt)
    self:cross()
    local onLeft = 0
    for _, body in ipairs(self.bodies) do
        onLeft = onLeft + (body.world == self.left and 1 or 0)
    end
    self:status(string.format('Left world %d bodies, right world %d, crossings %d, gravity %d and %d', onLeft, #self.bodies - onLeft, self.crossings, self.gravity.left, self.gravity.right))
end

function TwoWorlds:draw(area)
    graphics2d.drawRect({-800, -400, 800, 800}, '#FF1D2633')
    graphics2d.drawRect({0, -400, 800, 800}, '#FF241D33')
    graphics2d.drawLine(0, -400, 0, 400, 3, '#80FFFFFF', {layer = 1})
    graphics2d.drawText(nil, 'Gravity ' .. (self.gravity.left >= 0 and 'down' or 'up'), -400, -360, {size = 26, color = '#FF8FB0FF', anchor = {0.5, 0}, layer = 1})
    graphics2d.drawText(nil, 'Gravity ' .. (self.gravity.right >= 0 and 'down' or 'up'), 400, -360, {size = 26, color = '#FFC9A0FF', anchor = {0.5, 0}, layer = 1})
    graphics2d.drawCircle(-700, 300, 26, '#FF6B7A8F', {layer = 2})
    parts.drawAll(self.statics)
    parts.drawAll(self.bodies)
    self.grabs.left:draw()
    self.grabs.right:draw()
end

return TwoWorlds
