-- A bowling lane seen from above: a heavy ball that linear damping slows as it rolls, aimed and thrown with the pointer, the keys or a stick, ten light pins, gutter sensors and a score board of ten frames with the pins down in each. The first ball rolls by itself when the test opens.
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local input = require('haylen.input')
local m = require('haylen.math')
local physics2d = require('haylen.physics2d')
local ui = require('haylen.ui')

local Look = require('categories.physics.look')
local PhysicsTest = require('categories.physics.physics-test')

local Bowling = haylen.class('Bowling', PhysicsTest)

Bowling.code = [[
local world = physics2d.newWorld({gravity = {0, 0}})  -- Seen from above.
local ball = world:createBody({x = -650, y = 0, linearDamping = 0.12, angularDamping = 0.5})
ball:addCircle(30, {density = 6, restitution = 0.1, friction = 0.2})
pin:addCircle(15, {density = 0.4, restitution = 0.5, friction = 0.3})
ball:applyImpulse(math.cos(aim) * power * ball.mass, math.sin(aim) * power * ball.mass)
gutter:addBox(1380, 50, {sensor = true})  -- Up to the pit.]]

Bowling.actions = {
    {name = 'move', type = 'vector', up = {'key:w', 'key:up', 'button:dpadUp'}, down = {'key:s', 'key:down', 'button:dpadDown'}, left = {'key:a', 'key:left', 'button:dpadLeft'}, right = {'key:d', 'key:right', 'button:dpadRight'}, bindings = {'stick:left', 'virtualStick:move'}},
    {name = 'throw', type = 'button', bindings = {'key:space', 'button:south', 'virtual:throw'}},
}

Bowling.laneHalf = 140
Bowling.gutter = 50
Bowling.wall = 30
Bowling.start, Bowling.finish = -760, 740
Bowling.foulLine = -600
Bowling.pit = 90
Bowling.ballRadius = 30
Bowling.pinRadius = 15
Bowling.headPin = 400
Bowling.maxSpeed = 1500
Bowling.maxAim = 0.25
Bowling.chargeTime = 1.2
Bowling.settleTime = 2
Bowling.firstThrow = {aim = 0.02, power = 0.8}
-- The score board above the lane, with its top and the width and height of each frame.
Bowling.board = {top = -360, width = 116, height = 100}
Bowling.frames = 10
-- A pin seen a little from the front, as the half widths of its outline at heights up from its foot: its base, its belly, its neck with two red stripes and its head.
Bowling.pinProfile = {{0, 9}, {6, 13}, {14, 14.5}, {22, 12}, {28, 7}, {32, 5.5}, {36, 6.5}, {41, 8}, {46, 6}, {49, 2.5}}
Bowling.pinHeight = 50
Bowling.stripes = {30, 34.5}
Bowling.shadow = '#40101018'
-- The finger holes of the ball, as their distance from its center in radii and their angle.
Bowling.holes = {{0.42, 0}, {0.42, 0.55}, {0.38, -1.9}}
Bowling.ballMaterial = Look.tint('purple', '#FF7B57D6', '#FF553AA8', '#FFC4B2FF')

function Bowling:enter()
    self:frame{
        code = Bowling.code,
        hint = 'Drag back from the ball and let go to throw, or move it across the line with W and S, aim with A and D or the left stick and hold Space or the south button to charge the throw. R or the X button starts a new game.',
        controls = {
            ui.button{id = 'reset', text = 'Start a new game', onClick = function() self:newGame() end},
        },
        play = true,
        actions = Bowling.actions,
        view = {1600, 700},
        overlay = {
            ui.touchStick{action = 'move', radius = 110, mode = 'floating', touchOnly = true, anchor = 'bottomLeft', anchorTo = 'parent', margin = {0, 0, 24, 24}, width = 360, height = 300},
            ui.touchButton{action = 'throw', text = 'Throw', size = 150, touchOnly = true, anchor = 'bottomRight', anchorTo = 'parent', margin = {0, 24, 24, 0}},
        },
    }
    self.camera.position = {0, -70}
    self.world = physics2d.newWorld({gravity = {0, 0}})
    local alley = self.world:createBody({type = 'static'})
    local outer = Bowling.laneHalf + Bowling.gutter
    local length = Bowling.finish - Bowling.start
    alley:addBox(length, Bowling.wall, {offsetX = (Bowling.start + Bowling.finish) / 2, offsetY = -outer - Bowling.wall / 2, restitution = 0.3})
    alley:addBox(length, Bowling.wall, {offsetX = (Bowling.start + Bowling.finish) / 2, offsetY = outer + Bowling.wall / 2, restitution = 0.3})
    alley:addBox(Bowling.wall, outer * 2 + Bowling.wall * 2, {offsetX = Bowling.finish - Bowling.wall / 2, restitution = 0.1})
    -- The gutters end where the pit begins, so a ball that rolls into the pit is not in a gutter, and their sensors start a radius of the ball past the edge of the lane, so the ball is in a gutter once its middle leaves the lane.
    local pit = Bowling.finish - Bowling.wall - Bowling.pit
    local depth = Bowling.gutter - Bowling.ballRadius
    for _, side in ipairs({-1, 1}) do
        local gutter = self.world:createBody({type = 'static', x = (Bowling.start + pit) / 2, y = side * (outer - depth / 2)})
        gutter:addBox(pit - Bowling.start, depth, {sensor = true})
    end
    self.world.onSensorBegin = function(sensor, visitor, shapes)
        if visitor == self.ball then
            self.inGutter = sensor.y > 0 and 1 or -1
        end
    end
    self.boardRect = {0, 0, 0, 0}
    self.pinOutline, self.pinFill, self.pinShape, self.pinOrder = {}, {}, {}, {}
    self.pinStroke = {width = 1, color = Look.ink, closed = true, join = 'round', layer = 1}
    self.stripeStroke = {width = 1, color = '#FFF0473E', cap = 'round', layer = 1}
    self.stripe = {0, 0, 0, 0}
    self.boardShape = {radius = 10, color = '#FF2F3352', borderWidth = 0, borderColor = Look.ink, layer = -20}
    self:newGame()
    self.auto = true
end

function Bowling:newGame()
    self.scores, self.auto = {}, false
    self:rack()
end

function Bowling:rack()
    for _, body in ipairs(self.pins or {}) do
        body:destroy()
    end
    if self.ball then
        self.ball:destroy()
    end
    self.pins, self.spots = {}, {}
    for row = 0, 3 do
        for index = 0, row do
            local x, y = Bowling.headPin + row * 38, (index - row / 2) * 44
            local pin = self.world:createBody({x = x, y = y, linearDamping = 2.2, angularDamping = 2})
            pin:addCircle(Bowling.pinRadius, {density = 0.4, restitution = 0.5, friction = 0.3})
            self.pins[#self.pins + 1] = pin
            self.spots[#self.spots + 1] = {x, y}
        end
    end
    self.ball = self.world:createBody({x = Bowling.foulLine - 40, y = 0, linearDamping = 0.12, angularDamping = 0.5})
    self.ball:addCircle(Bowling.ballRadius, {density = 6, restitution = 0.1, friction = 0.2})
    self.state, self.aim, self.power, self.charging, self.inGutter, self.settle = 'aiming', 0, 0, false, false, 0
end

function Bowling:isDown(index)
    local pin, spot = self.pins[index], self.spots[index]
    return math.abs(pin.x - spot[1]) > 14 or math.abs(pin.y - spot[2]) > 14
end

function Bowling:pinsDown()
    local count = 0
    for index = 1, #self.pins do
        if self:isDown(index) then
            count = count + 1
        end
    end
    return count
end

function Bowling:throw()
    if self.state == 'aiming' and self.power > 0.05 then
        local speed = self.power * Bowling.maxSpeed * self.ball.mass
        self.ball:applyImpulse(math.cos(self.aim) * speed, math.sin(self.aim) * speed)
        self.state = 'rolling'
        if #self.scores == Bowling.frames then
            self.scores = {}
        end
    end
    self.power, self.charging, self.dragging, self.auto = 0, false, false, false
end

function Bowling:exit()
    Bowling.super.exit(self)
    input.clearVirtual()
end

function Bowling:update(dt)
    Bowling.super.update(self, dt)
    if input.pressed('reset') then
        self:newGame()
    end
    local pointer, ball = self.pointer, self.ball
    if self.state == 'aiming' then
        if pointer.pressed and not ui.usingPointer() then
            self.dragging, self.auto = true, false
        end
        if self.dragging then
            local dx, dy = ball.x - pointer.worldX, ball.y - pointer.worldY
            self.aim = m.clamp(math.atan(dy, math.max(dx, 1)), -Bowling.maxAim, Bowling.maxAim)
            self.power = math.min(1, math.sqrt(dx * dx + dy * dy) / 300)
            if pointer.released then
                self:throw()
            end
        end
        local moveX, moveY = input.vector('move')
        if moveY ~= 0 then
            local limit = Bowling.laneHalf - Bowling.ballRadius - 2
            ball:setTransform(Bowling.foulLine - 40, m.clamp(ball.y + moveY * 200 * dt, -limit, limit), 0)
            self.auto = false
        end
        if moveX ~= 0 then
            self.aim, self.auto = m.clamp(self.aim + moveX * 0.3 * dt, -Bowling.maxAim, Bowling.maxAim), false
        end
        if input.down('throw') then
            self.charging, self.auto = true, false
            self.power = math.min(1, self.power + dt / Bowling.chargeTime)
        elseif self.charging then
            self:throw()
        end
        -- The first ball of the test charges and rolls by itself.
        if self.auto then
            self.aim = Bowling.firstThrow.aim
            self.power = self.power + dt / Bowling.chargeTime
            if self.power >= Bowling.firstThrow.power then
                self:throw()
            end
        end
    end
    self:status(self:describe())
end

function Bowling:describe()
    if self.state == 'aiming' then
        local degrees = PhysicsTest.degrees(self.aim)
        local aim = degrees == 0 and 'straight down the lane' or string.format('%d degrees to the %s', math.abs(degrees), degrees < 0 and 'left' or 'right')
        return string.format('The ball waits on the line, aimed %s, with %d percent of its power.', aim, math.floor(self.power * 100 + 0.5))
    end
    local speed = math.floor(self.ball.velocity:length() + 0.5)
    return string.format('The ball rolls at %d units per second%s, and %d of the 10 pins are down.', speed, self.inGutter and ' down the gutter' or '', self:pinsDown())
end

-- A ball in a gutter drops into it and runs straight down it, and once the ball stops or reaches the pit the pins settle, are counted into the next frame and are set again.
function Bowling:fixedUpdate(step)
    local ball = self.ball
    if self.inGutter then
        local middle = self.inGutter * (Bowling.laneHalf + Bowling.gutter - Bowling.ballRadius)
        ball.velocity = {ball.velocity.x, (middle - ball.y) * 8}
    end
    if self.state == 'rolling' and (ball.x > Bowling.headPin + 200 or ball.velocity:length() < 10) then
        self.state, self.settle = 'settling', Bowling.settleTime
    elseif self.state == 'settling' then
        self.settle = self.settle - step
        if self.settle <= 0 then
            self.scores[#self.scores + 1] = self:pinsDown()
            self:rack()
        end
    end
    self:simulate(self.world, step)
end

-- The lane is maple boards with the guide arrows and the foul line, between two round gutters and the walls of the alley, with the pin deck and the dark pit at its end.
function Bowling:drawLane()
    local unit = Look.pixel
    local half, outer, wall = Bowling.laneHalf, Bowling.laneHalf + Bowling.gutter, Bowling.wall
    local start, finish = Bowling.start, Bowling.finish
    graphics2d.drawRect(graphics2d.canvasBounds(), '#FF221D3B', {layer = -30})
    graphics2d.drawShape({start, -outer - wall, finish - start, (outer + wall) * 2}, {radius = 12, color = '#FF7A4724', borderWidth = 4 * unit, borderColor = Look.ink, layer = -20})
    for _, side in ipairs({-1, 1}) do
        graphics2d.drawShape({start, side < 0 and -outer or half, finish - wall - start, Bowling.gutter}, {radius = Bowling.gutter / 2, color = '#FF474F66', borderWidth = 3 * unit, borderColor = Look.ink, layer = -19})
        graphics2d.drawShape({start + 6, (side < 0 and -outer or half) + Bowling.gutter * 0.35, finish - wall - start - 12, Bowling.gutter * 0.3}, {radius = 6, color = '#FF66708A', layer = -19})
    end
    graphics2d.drawRect({start, -half, Bowling.foulLine - start, half * 2}, '#FFD9A36A', {layer = -19})
    graphics2d.drawRect({Bowling.foulLine, -half, Bowling.headPin - 60 - Bowling.foulLine, half * 2}, '#FFF2C88E', {layer = -19})
    graphics2d.drawRect({Bowling.headPin - 60, -half, finish - wall - Bowling.pit - Bowling.headPin + 60, half * 2}, '#FFF8DAAE', {layer = -19})
    graphics2d.drawRect({finish - wall - Bowling.pit, -outer, Bowling.pit, outer * 2}, '#FF15172A', {layer = -19})
    for board = -half + 14, half - 1, 14 do
        graphics2d.drawLine(start, board, finish - wall - Bowling.pit, board, 1.5 * unit, '#26A86B3C', {layer = -18})
    end
    graphics2d.drawLine(Bowling.foulLine, -half, Bowling.foulLine, half, 6, '#FFF0473E', {layer = -18})
    for index = -2, 2 do
        local x, y = -260 + math.abs(index) * 40, index * 50
        graphics2d.drawShape({x - 14, y - 9, 28, 18}, {radius = 2, startAngle = math.pi * 0.75, sweep = math.pi * 0.5, color = '#FF7A4724', layer = -18})
        graphics2d.drawCircle(Bowling.foulLine + 70, y, 4, '#FF7A4724', {layer = -18})
    end
end

-- A standing pin stands up from its spot, seen a little from the front with its shadow, and a pin that is down lies on the lane pointing away from its spot, so the two read apart at a glance.
function Bowling:drawPin(index)
    local pin, spot = self.pins[index], self.spots[index]
    local x, y = pin.x, pin.y
    local down = self.state ~= 'aiming' and self:isDown(index)
    local angle, lift = 0, 0
    if down then
        angle, lift = math.atan(y - spot[2], x - spot[1]) + math.pi / 2, Bowling.pinHeight / 2
        graphics2d.drawEllipse(x + 3, y + 4, 16, 16, Bowling.shadow, {layer = 1})
    else
        graphics2d.drawEllipse(x + 5, y + 4, 16, 9, Bowling.shadow, {layer = 1})
    end
    local cos, sin = math.cos(angle), math.sin(angle)
    local profile, outline, fill = Bowling.pinProfile, self.pinOutline, self.pinFill
    local count = #profile
    for side = 0, 1 do
        for step = 1, count do
            local point = profile[side == 0 and step or count - step + 1]
            local width, height = point[2] * (side == 0 and 1 or -1), lift - point[1]
            local slot = side * count + step
            outline[slot * 2 - 1], outline[slot * 2] = x + width * cos - height * sin, y + width * sin + height * cos
            local inner = width * 0.78 - 1.5
            fill[slot] = fill[slot] or {0, 0}
            fill[slot][1], fill[slot][2] = x + inner * cos - height * sin, y + inner * sin + height * cos
        end
    end
    local shape = self.pinShape
    for slot = 1, count * 2 do
        shape[slot] = shape[slot] or {0, 0}
        shape[slot][1], shape[slot][2] = outline[slot * 2 - 1], outline[slot * 2]
    end
    graphics2d.drawPolygon(shape, '#FFD9D0BC', {layer = 1})
    graphics2d.drawPolygon(fill, '#FFFBF8F2', {layer = 1})
    for _, height in ipairs(Bowling.stripes) do
        local half, level = 6, lift - height
        local stripe = self.stripe
        stripe[1], stripe[2] = x - half * cos - level * sin, y - half * sin + level * cos
        stripe[3], stripe[4] = x + half * cos - level * sin, y + half * sin + level * cos
        self.stripeStroke.width = 2.5
        graphics2d.drawStroke(stripe, self.stripeStroke)
    end
    self.pinStroke.width = 3 * Look.pixel
    graphics2d.drawStroke(outline, self.pinStroke)
end

-- Standing pins draw from the back of the rack to its front, so each stands in front of the pins behind it.
function Bowling:drawPins()
    local order = self.pinOrder
    for index = 1, #self.pins do
        order[index] = index
    end
    for index = #self.pins + 1, #order do
        order[index] = nil
    end
    table.sort(order, function(a, b) return self.pins[a].y < self.pins[b].y end)
    for _, index in ipairs(order) do
        self:drawPin(index)
    end
end

-- The ball is a purple disc with its three finger holes, which turn with it.
function Bowling:drawBall()
    local ball = self.ball
    local r = Bowling.ballRadius
    Look.disc(ball.x, ball.y, r, Bowling.ballMaterial, 2)
    local angle = ball.rotation
    for _, hole in ipairs(Bowling.holes) do
        local a = angle + hole[2]
        graphics2d.drawCircle(ball.x + math.cos(a) * r * hole[1], ball.y + math.sin(a) * r * hole[1], r * 0.14, '#FF2F2B45', {layer = 2})
    end
end

-- The score board holds ten frames with the pins down in each, an X for a strike and a dash for none, and the total.
function Bowling:drawBoard()
    local board = Bowling.board
    local unit = Look.pixel
    local total = 0
    local width = board.width * Bowling.frames + 180
    local left = -width / 2
    local rect, shape = self.boardRect, self.boardShape
    rect[1], rect[2], rect[3], rect[4] = left - 12, board.top - 12, width + 24, board.height + 24
    shape.color, shape.borderWidth = '#FF7A4724', 4 * unit
    graphics2d.drawShape(rect, shape)
    shape.borderWidth = 2 * unit
    for frame = 1, Bowling.frames do
        local score = self.scores[frame]
        total = total + (score or 0)
        local x = left + (frame - 1) * board.width
        rect[1], rect[2], rect[3], rect[4] = x + 4, board.top, board.width - 8, board.height
        local current = frame == #self.scores + 1
        shape.color = current and '#FF3A4170' or '#FF2F3352'
        graphics2d.drawShape(rect, shape)
        self:tag(tostring(frame), x + board.width / 2, board.top + 22, {size = 16, color = '#FFA9BCD3'})
        -- The frame in play counts the pins down as they fall, in the color of a count that is not final yet.
        local live = current and self.state ~= 'aiming'
        if live then
            score = self:pinsDown()
        end
        if score then
            local text = score == 10 and 'X' or score == 0 and '-' or tostring(score)
            local color = live and '#FFC9D3E2' or score == 10 and '#FFFFC93C' or '#FFFFFFFF'
            self:tag(text, x + board.width / 2, board.top + 64, {size = 34, color = color})
        end
    end
    rect[1], rect[2], rect[3], rect[4] = left + Bowling.frames * board.width + 4, board.top, 172, board.height
    shape.color = '#FF15172A'
    graphics2d.drawShape(rect, shape)
    self:tag('Total', rect[1] + 86, board.top + 22, {size = 16, color = '#FFA9BCD3'})
    self:tag(tostring(total), rect[1] + 86, board.top + 64, {size = 38, color = '#FFFFC93C'})
end

function Bowling:draw(area)
    self:drawLane()
    self:drawPins()
    self:drawBall()
    if self.state == 'aiming' then
        local ball, dx, dy = self.ball, math.cos(self.aim), math.sin(self.aim)
        local reach = 70 + self.power * 300
        Look.line(ball.x + dx * 44, ball.y + dy * 44, ball.x + dx * reach, ball.y + dy * reach, 8, '#FFFFC93C', 3)
        local tipX, tipY = ball.x + dx * (reach + 16), ball.y + dy * (reach + 16)
        graphics2d.drawShape({tipX - 18, tipY - 18, 36, 36}, {rotation = self.aim, startAngle = math.pi * 0.75, sweep = math.pi / 2, color = '#FFFFC93C', borderWidth = 3 * Look.pixel, borderColor = Look.ink, layer = 3})
    end
    self:drawBoard()
end

return Bowling
