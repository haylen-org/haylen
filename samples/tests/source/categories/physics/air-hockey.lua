-- Air hockey seen from above: kinematic paddles that `body:moveTo` drives, one following the pointer, the keys or a stick and one played by the computer, a puck with `bullet = true` inside one rounded chain with the goals cut into its ends, and goal sensors that keep the score. The computer plays both paddles until the player moves.
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local input = require('haylen.input')
local m = require('haylen.math')
local physics2d = require('haylen.physics2d')
local ui = require('haylen.ui')

local Look = require('categories.physics.look')
local PhysicsTest = require('categories.physics.physics-test')

local AirHockey = haylen.class('AirHockey', PhysicsTest)

AirHockey.code = [[
local paddle = world:createBody({type = 'kinematic', x = -500, y = 0})
paddle:moveTo(targetX, targetY)  -- Once per fixed step, so the paddle hits the puck instead of teleporting.
local puck = world:createBody({linearDamping = 0.15, bullet = true})
rink:addChain(outline, true, {restitution = 0.9, friction = 0})  -- One loop with the goals cut into its ends.
net:addBox(40, 220, {sensor = true})
world.onSensorBegin = function(sensor, visitor) if visitor == puck then score = score + 1 end end]]

AirHockey.actions = {
    {name = 'move', type = 'vector', up = {'key:w', 'key:up', 'button:dpadUp'}, down = {'key:s', 'key:down', 'button:dpadDown'}, left = {'key:a', 'key:left', 'button:dpadLeft'}, right = {'key:d', 'key:right', 'button:dpadRight'}, bindings = {'stick:left', 'virtualStick:move'}},
}

AirHockey.halfWidth, AirHockey.halfHeight = 700, 380
AirHockey.corner = 90
-- Half the height of a goal mouth and how deep the goal goes into the frame.
AirHockey.goal, AirHockey.goalDepth = 110, 60
AirHockey.rail = 46
AirHockey.paddleRadius, AirHockey.puckRadius = 40, 26
AirHockey.paddleSpeed = 1800
AirHockey.computerSpeed = 900
AirHockey.keySpeed = 900
AirHockey.winningScore = 7
AirHockey.serveDelay = 1
AirHockey.holeSpacing = 40
AirHockey.puckMaterial = Look.tint('yellow', '#FFFFC93C', '#FFD99A12', '#FFFFF0A0')

function AirHockey:enter()
    self:frame{
        code = AirHockey.code,
        hint = 'Move your paddle on the left half with the mouse, a finger, the keys or the left stick, and hit the puck into the goal on the right. The first to 7 wins. R or the X button starts a new match.',
        controls = {
            ui.button{id = 'reset', text = 'New match', onClick = function() self:build() end},
        },
        play = true,
        actions = AirHockey.actions,
        view = {2000, 900},
        overlay = {
            ui.touchStick{action = 'move', radius = 110, mode = 'floating', touchOnly = true, anchor = 'bottomLeft', anchorTo = 'parent', margin = {0, 0, 24, 24}, width = 360, height = 300},
        },
    }
    self.holes = self:airHoles()
    self.outline = {}
    for _, point in ipairs(self:rinkOutline()) do
        self.outline[#self.outline + 1] = point[1]
        self.outline[#self.outline + 1] = point[2]
    end
    self:build()
end

-- The rink runs counter-clockwise on the screen, so the chain holds the puck on its inner side, and each end turns into its goal and out again.
function AirHockey:rinkOutline()
    local width, height, corner = AirHockey.halfWidth, AirHockey.halfHeight, AirHockey.corner
    local goal, depth = AirHockey.goal, AirHockey.goalDepth
    local points = {}
    local function arc(cx, cy, from, to)
        for step = 0, 6 do
            local angle = from + (to - from) * step / 6
            points[#points + 1] = {cx + corner * math.cos(angle), cy + corner * math.sin(angle)}
        end
    end
    for _, point in ipairs({{width, goal}, {width + depth, goal}, {width + depth, -goal}, {width, -goal}}) do
        points[#points + 1] = point
    end
    arc(width - corner, -height + corner, 0, -math.pi / 2)
    arc(-width + corner, -height + corner, -math.pi / 2, -math.pi)
    for _, point in ipairs({{-width, -goal}, {-width - depth, -goal}, {-width - depth, goal}, {-width, goal}}) do
        points[#points + 1] = point
    end
    arc(-width + corner, height - corner, math.pi, math.pi / 2)
    arc(width - corner, height - corner, math.pi / 2, 0)
    return points
end

-- The air holes of the table top, as one flat list of points that a single call draws.
function AirHockey:airHoles()
    local holes = {}
    local width, height, corner, spacing = AirHockey.halfWidth - 20, AirHockey.halfHeight - 20, AirHockey.corner, AirHockey.holeSpacing
    for x = -width, width, spacing do
        for y = -height + 10, height - 10, spacing do
            local cx, cy = math.max(math.abs(x) - (width - corner), 0), math.max(math.abs(y) - (height - corner), 0)
            if cx * cx + cy * cy < (corner - 10) ^ 2 then
                holes[#holes + 1] = x
                holes[#holes + 1] = y
            end
        end
    end
    return holes
end

function AirHockey:build()
    self.world = physics2d.newWorld({gravity = {0, 0}, maxSpeed = 2400})
    local rink = self.world:createBody({type = 'static'})
    rink:addChain(self:rinkOutline(), true, {restitution = 0.9, friction = 0})
    for _, goal in ipairs({{-1, 'computer'}, {1, 'player'}}) do
        local net = self.world:createBody({type = 'static', x = goal[1] * (AirHockey.halfWidth + 40), y = 0})
        net:addBox(40, AirHockey.goal * 2, {sensor = true})
        net.data = {scorer = goal[2]}
    end

    self.puck = self.world:createBody({linearDamping = 0.15, bullet = true})
    self.puck:addCircle(AirHockey.puckRadius, {restitution = 0.9, friction = 0})
    self.puck.velocity = {-520, 300}
    self.player = self:paddle(-500)
    self.computer = self:paddle(500)
    self.target = {x = -500, y = 0}
    self.scores = {player = 0, computer = 0}
    self.serve, self.message, self.demo = 0, nil, true

    self.world.onSensorBegin = function(sensor, visitor, shapes)
        if visitor == self.puck and self.serve <= 0 then
            local scorer = sensor.data.scorer
            self.scores[scorer] = self.scores[scorer] + 1
            visitor.velocity = {0, 0}
            self.serve, self.serveSide = AirHockey.serveDelay, scorer == 'player' and 1 or -1
            if self.scores[scorer] >= AirHockey.winningScore then
                self.message = scorer == 'player' and 'You win the match' or 'The computer wins the match'
                self.serve = AirHockey.serveDelay * 3
            end
        end
    end
end

function AirHockey:paddle(x)
    local body = self.world:createBody({type = 'kinematic', x = x, y = 0})
    body:addCircle(AirHockey.paddleRadius, {restitution = 0.6})
    return body
end

-- Moves a paddle toward a point of its own half, no faster than its top speed.
function AirHockey:drive(body, x, y, minimumX, maximumX, speed, step)
    x = m.clamp(x, minimumX, maximumX)
    y = m.clamp(y, -AirHockey.halfHeight + AirHockey.paddleRadius, AirHockey.halfHeight - AirHockey.paddleRadius)
    local dx, dy = x - body.x, y - body.y
    local length = math.sqrt(dx * dx + dy * dy)
    local reach = speed * step
    if length > reach then
        dx, dy = dx / length * reach, dy / length * reach
    end
    body:moveTo(body.x + dx, body.y + dy)
end

function AirHockey:exit()
    AirHockey.super.exit(self)
    input.clearVirtual()
end

function AirHockey:update(dt)
    AirHockey.super.update(self, dt)
    if input.pressed('reset') then
        self:build()
    end
    local pointer = self.pointer
    if self.lastX and (pointer.x ~= self.lastX or pointer.y ~= self.lastY) and not ui.usingPointer() then
        self.target.x, self.target.y, self.demo = pointer.worldX, pointer.worldY, false
    end
    self.lastX, self.lastY = pointer.x, pointer.y
    local moveX, moveY = input.vector('move')
    if moveX ~= 0 or moveY ~= 0 then
        if self.demo then
            self.target.x, self.target.y, self.demo = self.player.x, self.player.y, false
        end
        self.target.x = m.clamp(self.target.x + moveX * AirHockey.keySpeed * dt, -AirHockey.halfWidth, 0)
        self.target.y = m.clamp(self.target.y + moveY * AirHockey.keySpeed * dt, -AirHockey.halfHeight, AirHockey.halfHeight)
    end
    local speed = math.floor(self.puck.velocity:length() + 0.5)
    local who = self.demo and 'The computer plays both paddles until you move yours' or string.format('You have %d and the computer %d', self.scores.player, self.scores.computer)
    self:status(string.format('%s, and the puck slides at %d units per second.', who, speed))
end

-- The computer defends its goal while the puck is away and goes for the puck once it crosses the middle, and plays the left paddle the same way until the player moves it.
function AirHockey:fixedUpdate(step)
    local puck = self.puck
    local width, radius = AirHockey.halfWidth, AirHockey.paddleRadius
    if self.demo then
        if puck.x < 0 then
            self:drive(self.player, puck.x - 30, puck.y, -width + radius, -radius, AirHockey.computerSpeed, step)
        else
            self:drive(self.player, -width + 140, puck.y * 0.6, -width + radius, -radius, AirHockey.computerSpeed, step)
        end
    else
        self:drive(self.player, self.target.x, self.target.y, -width + radius, -radius, AirHockey.paddleSpeed, step)
    end
    if puck.x > 0 then
        self:drive(self.computer, puck.x + 30, puck.y, radius, width - radius, AirHockey.computerSpeed, step)
    else
        self:drive(self.computer, width - 140, puck.y * 0.6, radius, width - radius, AirHockey.computerSpeed, step)
    end
    if self.serve > 0 then
        self.serve = self.serve - step
        if self.serve <= 0 then
            if self.message then
                self.scores, self.message = {player = 0, computer = 0}, nil
            end
            puck:setTransform(self.serveSide * 200, 0, 0)
            puck.velocity = {0, 0}
        end
    end
    self:simulate(self.world, step)
end

-- The table is a white top with air holes and the lines of the game inside a dark rail, whose ends hold the goals as slots cut into it.
function AirHockey:drawTable()
    local unit = Look.pixel
    local width, height, rail, depth = AirHockey.halfWidth, AirHockey.halfHeight, AirHockey.rail, AirHockey.goalDepth
    local corner = AirHockey.corner
    graphics2d.drawRect(graphics2d.canvasBounds(), '#FF221D3B', {layer = -30})
    graphics2d.drawShape({-width - depth - 30, -height - rail + 4, (width + depth) * 2 + 60, (height + rail) * 2 + 20}, {radius = corner + 30, color = '#66000000', softness = 40, layer = -21})
    graphics2d.drawShape({-width - depth - 14, -height - rail, (width + depth + 14) * 2, (height + rail) * 2}, {radius = corner + rail, color = '#FF2F3352', borderWidth = 5 * unit, borderColor = Look.ink, layer = -20})
    graphics2d.drawShape({-width, -height, width * 2, height * 2}, {radius = corner, color = '#FFEAF3FB', layer = -19})
    for _, sign in ipairs({-1, 1}) do
        graphics2d.drawShape({sign > 0 and width - 10 or -width - depth, -AirHockey.goal, depth + 10, AirHockey.goal * 2}, {radius = 8, color = '#FF15172A', layer = -19})
    end
    graphics2d.drawPoints(self.holes, {radius = 2.6, color = '#FFC5D4E6', layer = -18})

    graphics2d.drawLine(0, -height, 0, height, 8, '#B3F0473E', {layer = -18})
    graphics2d.drawRing(0, 0, 120, 6, '#B3F0473E', {layer = -18})
    graphics2d.drawCircle(0, 0, 14, '#B3F0473E', {layer = -18})
    for _, sign in ipairs({-1, 1}) do
        local x = sign * width
        graphics2d.drawLine(sign * width * 0.55, -height, sign * width * 0.55, height, 5, '#802F86F6', {layer = -18})
        graphics2d.drawArc(x, 0, 180, 6, sign < 0 and -math.pi / 2 or math.pi / 2, sign < 0 and math.pi / 2 or math.pi * 1.5, '#B32F86F6', {layer = -18})
    end
    graphics2d.drawStroke(self.outline, {width = 10 + 7 * unit, color = Look.ink, join = 'round', closed = true, layer = -17})
    graphics2d.drawStroke(self.outline, {width = 10, color = '#FFA9BCD3', join = 'round', closed = true, layer = -17})
end

-- A paddle is a disc of the color of its side with a knob on top, which the hand holds.
function AirHockey:drawPaddle(body, material)
    Look.disc(body.x, body.y, AirHockey.paddleRadius, material, 1)
    Look.disc(body.x, body.y, 17, material, 2)
end

-- A score board stands beside each end of the table, in the color of the paddle of its side.
function AirHockey:drawScore(name, score, x, plate)
    local unit = Look.pixel
    graphics2d.drawShape({x - 90, -130, 180, 220}, {radius = 22, color = plate, borderWidth = 4 * unit, borderColor = Look.ink, layer = -20})
    self:tag(name, x, -92, {size = 24})
    self:tag(tostring(score), x, 10, {size = 96})
end

function AirHockey:draw(area)
    self:drawTable()
    self:drawPaddle(self.player, 'red')
    self:drawPaddle(self.computer, 'blue')
    local puck = self.puck
    Look.disc(puck.x, puck.y, AirHockey.puckRadius, AirHockey.puckMaterial, 1)
    graphics2d.drawRing(puck.x, puck.y, 15, 3 * Look.pixel, '#FFD99A12', {layer = 1})
    local edge = AirHockey.halfWidth + AirHockey.goalDepth + 130
    self:drawScore('You', self.scores.player, -edge, '#FFB9302A')
    self:drawScore('Computer', self.scores.computer, edge, '#FF1E5FB8')
    if self.message then
        self:label(self.message, 0, -40, {size = 44})
    end
end

return AirHockey
