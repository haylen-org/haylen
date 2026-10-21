-- A pool table seen from above in a world without gravity: balls that slow down by linear damping and fall asleep early, a cue aimed with the pointer or a stick with a guide from `world:castCircle`, cushions cut at the pockets and pocket sensors that take the balls and count them. The cue breaks the rack by itself when the test opens.
local graphics = require('haylen.graphics')
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local input = require('haylen.input')
local physics2d = require('haylen.physics2d')
local ui = require('haylen.ui')

local Look = require('categories.physics.look')
local PhysicsTest = require('categories.physics.physics-test')

local Pool = haylen.class('Pool', PhysicsTest)

Pool.code = [[
local world = physics2d.newWorld({gravity = {0, 0}})
local ball = world:createBody({x = 300, y = 0, linearDamping = 0.9, angularDamping = 1.5, sleepThreshold = 12})
ball:addCircle(22, {restitution = 0.95, friction = 0.05, category = kBall})
cushion:addPolygon({{44, -330}, {608, -330}, {638, -360}, {30, -360}}, {restitution = 0.8})  -- Cut at the pockets.
local hit = world:castCircle(cue.x, cue.y, 22, dx * 1600, dy * 1600, {mask = kCushion | kBall})  -- Where the guide ends.
pocket:addCircle(30, {sensor = true, category = kPocket})
cue:applyImpulse(dx * power * cue.mass, dy * power * cue.mass)]]

Pool.actions = {
    {name = 'turn', type = 'axis', positive = {'key:d', 'key:right', 'button:dpadRight'}, negative = {'key:a', 'key:left', 'button:dpadLeft'}},
    {name = 'aim', type = 'vector', bindings = {'stick:left', 'virtualStick:aim'}},
    {name = 'shoot', type = 'button', bindings = {'key:space', 'button:south', 'virtual:shoot'}},
}

Pool.cushionCategory, Pool.ballCategory, Pool.cueCategory, Pool.pocketCategory = 1, 2, 4, 8
-- Half the size of the cloth inside the cushions, the depth of the cushions and of the wooden rail around them.
Pool.halfWidth, Pool.halfHeight = 660, 330
Pool.cushionDepth = 30
Pool.railDepth = 50
Pool.radius = 22
Pool.maxSpeed = 2400
Pool.chargeTime = 1.2
Pool.ready = 40
Pool.head = {-330, 0}
Pool.breakPower = 0.85
-- The colors of the balls 1 to 8 with their shade, and the balls 9 to 15 take the colors of 1 to 7 on a stripe.
Pool.colors = {
    {'#FFFFC93C', '#FFD99A12'},
    {'#FF2F86F6', '#FF1E5FB8'},
    {'#FFF0473E', '#FFB9302A'},
    {'#FF9A6BE0', '#FF6E44B0'},
    {'#FFFF9A3C', '#FFD06E1E'},
    {'#FF4CB866', '#FF2E8A4A'},
    {'#FFB2433F', '#FF7E2A2A'},
    {'#FF2F2B45', '#FF1A1830'},
}
Pool.white = {'#FFFBF6EA', '#FFD9D0BC'}
-- The stretches of the cue from its tip, with where each starts and ends, how wide it is there and its color.
Pool.cueParts = {{0, 8, 7, 7.4, '#FF4A78C8'}, {8, 22, 7.4, 8, '#FFF2EEDF'}, {22, 300, 8, 11, '#FFF2D4A0'}, {300, 470, 11, 14, '#FF7A4724'}}
Pool.cueLength = 470
-- The pearl sights along the long rails, as shares of half the length of the cloth, and along the short rails, as shares of half its width.
Pool.longSights = {-0.75, -0.5, -0.25, 0.25, 0.5, 0.75}
Pool.shortSights = {-0.5, 0, 0.5}

function Pool:enter()
    self:frame{
        code = Pool.code,
        hint = 'Drag back from the cue ball and let go to shoot, or turn the cue with A and D, the arrows or the left stick and hold Space or the south button to charge the shot. R or the X button racks the balls again.',
        controls = {
            ui.button{id = 'rack', text = 'Rack the balls', onClick = function() self:build() end},
        },
        play = true,
        actions = Pool.actions,
        view = {1960, 880},
        overlay = {
            ui.touchStick{action = 'aim', radius = 110, mode = 'floating', touchOnly = true, anchor = 'bottomLeft', anchorTo = 'parent', margin = {0, 0, 24, 24}, width = 360, height = 300},
            ui.touchButton{action = 'shoot', text = 'Shoot', size = 150, touchOnly = true, anchor = 'bottomRight', anchorTo = 'parent', margin = {0, 24, 24, 0}},
        },
    }
    local cloth = graphics.newImage(256, 128)
    cloth:fillGradient({kind = 'radial', radius = 0.62, stops = {{0, '#FF46A867'}, {0.7, '#FF3A9459'}, {1, '#FF2A7445'}}})
    self.cloth = graphics.newTexture(cloth, {filter = 'linear'})
    self.ballShape = {kind = 'ellipse', rotation = 0, color = '#FFFFFFFF', borderWidth = 0, borderColor = Look.ink, layer = 1}
    self.ballRect = {0, 0, 0, 0}
    self.numberStyle = {size = 1, color = Look.ink, anchor = {0.5, 0.52}, layer = 1}
    self.cue = {{0, 0}, {0, 0}, {0, 0}, {0, 0}}
    self.cueOrder = {layer = 6}
    self.sight = {kind = 'rectangle', rotation = math.pi / 4, color = '#FFF2EEDF', borderWidth = 0, borderColor = Look.ink, layer = -20}
    self.sightRect = {0, 0, 14, 14}
    self:build()
    self.breaking = true
end

function Pool:build()
    self.world = physics2d.newWorld({gravity = {0, 0}})
    local width, height, depth = Pool.halfWidth, Pool.halfHeight, Pool.cushionDepth
    local rails = self.world:createBody({type = 'static'})

    -- Six cushions, each cut at 45 degrees where it meets a pocket so the mouths open toward the cloth.
    self.cushions = {}
    for _, sign in ipairs({-1, 1}) do
        for _, side in ipairs({-1, 1}) do
            local y, outer = sign * height, sign * (height + depth)
            self.cushions[#self.cushions + 1] = {{side * 44, y}, {side * (width - 52), y}, {side * (width - 22), outer}, {side * 30, outer}}
        end
        local x, outer = sign * width, sign * (width + depth)
        self.cushions[#self.cushions + 1] = {{x, -(height - 52)}, {outer, -(height - 22)}, {outer, height - 22}, {x, height - 52}}
    end
    for index, points in ipairs(self.cushions) do
        rails:addPolygon(points, {restitution = 0.8, friction = 0.2, category = Pool.cushionCategory})
        self.cushions[index] = Look.newShape(points, Look.tint('rubber', '#FF3A9459', '#FF266C3E', '#FF7BCB8E'))
    end
    local edgeX, edgeY = width + depth + 30, height + depth + 30
    rails:addChain({{-edgeX, -edgeY}, {-edgeX, edgeY}, {edgeX, edgeY}, {edgeX, -edgeY}}, true, {category = Pool.cushionCategory})

    self.pockets = {}
    for _, spot in ipairs({{-width - 20, -height - 20}, {0, -height - 34}, {width + 20, -height - 20}, {-width - 20, height + 20}, {0, height + 34}, {width + 20, height + 20}}) do
        local pocket = self.world:createBody({type = 'static', x = spot[1], y = spot[2]})
        pocket:addCircle(30, {sensor = true, category = Pool.pocketCategory})
        self.pockets[#self.pockets + 1] = pocket
    end

    -- The rack is a triangle of fifteen balls whose rows touch, with the eight in the middle of the third row.
    self.balls = {}
    local numbers = {1, 9, 2, 10, 8, 3, 11, 4, 12, 5, 13, 6, 14, 7, 15}
    local count, spacing = 0, Pool.radius * 2 + 0.5
    for row = 0, 4 do
        for column = 0, row do
            count = count + 1
            local number = numbers[count]
            self:addBall(300 + row * spacing * 0.866, (column - row / 2) * spacing, number, Pool.ballCategory)
        end
    end
    self.cueBall = self:addBall(Pool.head[1], Pool.head[2], 0, Pool.cueCategory)
    self.aim, self.power, self.pocketed, self.shots, self.charging, self.breaking = 0, 0, 0, 0, false, false

    self.world.onSensorBegin = function(sensor, visitor, shapes)
        if not (visitor and visitor.type == 'dynamic') then
            return
        end
        if visitor == self.cueBall then
            visitor:setTransform(Pool.head[1], Pool.head[2], 0)
            visitor.velocity = {0, 0}
        else
            self.pocketed = self.pocketed + 1
            visitor:destroy()
        end
    end
end

function Pool:addBall(x, y, number, category)
    local ball = self.world:createBody({x = x, y = y, linearDamping = 0.9, angularDamping = 1.5, sleepThreshold = 12})
    ball:addCircle(Pool.radius, {restitution = 0.95, friction = 0.05, category = category})
    ball.data = {number = number, label = tostring(number)}
    self.balls[#self.balls + 1] = ball
    return ball
end

function Pool:isReady()
    return self.cueBall.velocity:length() < Pool.ready
end

function Pool:shoot()
    if self:isReady() and self.power > 0.03 then
        local speed = self.power * Pool.maxSpeed * self.cueBall.mass
        self.cueBall:applyImpulse(math.cos(self.aim) * speed, math.sin(self.aim) * speed)
        self.shots = self.shots + 1
    end
    self.power, self.charging, self.dragging, self.breaking = 0, false, false, false
end

function Pool:exit()
    Pool.super.exit(self)
    input.clearVirtual()
end

function Pool:update(dt)
    Pool.super.update(self, dt)
    if input.pressed('reset') then
        self:build()
    end
    local pointer, cue = self.pointer, self.cueBall
    if pointer.pressed and not ui.usingPointer() then
        self.dragging, self.breaking = true, false
    end
    if self.dragging then
        local dx, dy = cue.x - pointer.worldX, cue.y - pointer.worldY
        self.aim = math.atan(dy, dx)
        self.power = math.min(1, math.sqrt(dx * dx + dy * dy) / 300)
        if pointer.released then
            self:shoot()
        end
    end

    local aimX, aimY = input.vector('aim')
    if aimX * aimX + aimY * aimY > 0.36 then
        self.aim, self.breaking = math.atan(aimY, aimX), false
    end
    self.aim = self.aim + input.value('turn') * 1.2 * dt
    if input.down('shoot') then
        self.charging, self.breaking = true, false
        self.power = math.min(1, self.power + dt / Pool.chargeTime)
    elseif self.charging then
        self:shoot()
    end

    -- The opening break draws the cue back by itself and strikes once it reaches its power.
    if self.breaking then
        self.power = self.power + dt / Pool.chargeTime
        if self.power >= Pool.breakPower then
            self:shoot()
        end
    end

    local rolling = self.world.awakeBodyCount
    local still = rolling == 0 and 'every ball rests' or string.format('%d %s still rolling', rolling, rolling == 1 and 'ball is' or 'balls are')
    self:status(string.format('%d of 15 balls are pocketed after %d %s, and %s.', self.pocketed, self.shots, self.shots == 1 and 'shot' or 'shots', still))
end

function Pool:fixedUpdate(step)
    self:simulate(self.world, step)
end

-- The table is a wooden rail with pearl sights around the cloth, which darkens toward the cushions, and pockets with a brass rim cut into the corners and the middle of the long sides.
function Pool:drawTable()
    local unit = Look.pixel
    local width, height = Pool.halfWidth + Pool.cushionDepth, Pool.halfHeight + Pool.cushionDepth
    local rail = Pool.railDepth
    graphics2d.drawRect(graphics2d.canvasBounds(), '#FF2B2440', {layer = -30})
    graphics2d.drawShape({-width - rail - 30, -height - rail - 10, (width + rail) * 2 + 60, (height + rail) * 2 + 60}, {radius = 70, color = '#59000000', softness = 40, layer = -21})
    graphics2d.drawShape({-width - rail, -height - rail, (width + rail) * 2, (height + rail) * 2}, {radius = 44, color = '#FF7A4724', borderWidth = 5 * unit, borderColor = Look.ink, layer = -20})
    graphics2d.drawShape({-width - rail + 8, -height - rail + 8, (width + rail) * 2 - 16, (height + rail) * 2 - 16}, {radius = 38, color = '#FFA86B3C', layer = -20})
    graphics2d.drawShape({-width - rail + 16, -height - rail + 14, (width + rail) * 2 - 34, 10}, {radius = 5, color = '#59FFD39A', layer = -20})
    graphics2d.drawShape({-width, -height, width * 2, height * 2}, {texture = self.cloth, borderWidth = 3 * unit, borderColor = '#FF1E5A34', layer = -19})
    graphics2d.drawLine(Pool.head[1], -Pool.halfHeight, Pool.head[1], Pool.halfHeight, 3 * unit, '#4DFFFFFF', {layer = -18})
    graphics2d.drawCircle(300, 0, 6 * unit, '#80FFFFFF', {layer = -18})

    self.sight.borderWidth = 2 * unit
    for _, share in ipairs(Pool.longSights) do
        self:drawSight(share * Pool.halfWidth, -height - rail / 2)
        self:drawSight(share * Pool.halfWidth, height + rail / 2)
    end
    for _, share in ipairs(Pool.shortSights) do
        self:drawSight(-width - rail / 2, share * Pool.halfHeight)
        self:drawSight(width + rail / 2, share * Pool.halfHeight)
    end
    for _, pocket in ipairs(self.pockets) do
        graphics2d.drawShape({pocket.x - 40, pocket.y - 40, 80, 80}, {kind = 'ellipse', color = '#FF15121F', borderWidth = 8, borderColor = '#FFE0B04A', layer = -17})
        graphics2d.drawShape({pocket.x - 40, pocket.y - 40, 80, 80}, {kind = 'ellipse', color = '#00000000', borderWidth = 3 * unit, borderColor = Look.ink, layer = -17})
    end
    for _, cushion in ipairs(self.cushions) do
        Look.drawShape(cushion, -16)
    end
end

function Pool:drawSight(x, y)
    self.sightRect[1], self.sightRect[2] = x - 7, y - 7
    graphics2d.drawShape(self.sightRect, self.sight)
end

-- A ball is a disc of its color with the bold outline, a white stripe ball shows its color as a band, and each one carries its number on a white spot.
function Pool:drawBall(ball)
    local unit = Look.pixel
    local number = ball.data.number
    local r = Pool.radius
    local colors = number == 0 and Pool.white or number > 8 and Pool.white or Pool.colors[number]
    local shape, rect = self.ballShape, self.ballRect
    rect[1], rect[2], rect[3], rect[4] = ball.x - r, ball.y - r, r * 2, r * 2
    shape.color, shape.borderWidth, shape.rotation = colors[2], Look.outlinePixels * unit, 0
    graphics2d.drawShape(rect, shape)
    rect[1], rect[2], rect[3], rect[4] = ball.x - r + 1.5, ball.y - r + 1.5, r * 2 - 6, r * 2 - 6
    shape.color, shape.borderWidth = colors[1], 0
    graphics2d.drawShape(rect, shape)
    if number > 8 then
        local band = Pool.colors[number - 8]
        rect[1], rect[2], rect[3], rect[4] = ball.x - r + 1, ball.y - r * 0.56, r * 2 - 2, r * 1.12
        shape.color, shape.rotation = band[1], ball.rotation
        graphics2d.drawShape(rect, shape)
    end
    if number > 0 then
        rect[1], rect[2], rect[3], rect[4] = ball.x - r * 0.46, ball.y - r * 0.46, r * 0.92, r * 0.92
        shape.color, shape.rotation = Pool.white[1], 0
        graphics2d.drawShape(rect, shape)
        self.numberStyle.size = r * 0.72
        graphics2d.drawText(self.labelFont, ball.data.label, ball.x, ball.y, self.numberStyle)
    end
    rect[1], rect[2], rect[3], rect[4] = ball.x - r * 0.62, ball.y - r * 0.66, r * 0.5, r * 0.28
    shape.color, shape.rotation = '#B3FFFFFF', -0.75
    graphics2d.drawShape(rect, shape)
end

-- The cue tapers from its blue tip and white ferrule along a maple shaft to a dark butt, and pulls back from the ball with the power of the shot.
function Pool:drawCue(dx, dy)
    local start = Pool.radius + 10 + self.power * 120
    self:cueQuad(dx, dy, start, 0, Pool.cueLength, Pool.cueParts[1][3], Pool.cueParts[#Pool.cueParts][4], Look.outlinePixels * Look.pixel, Look.ink)
    for _, part in ipairs(Pool.cueParts) do
        self:cueQuad(dx, dy, start, part[1], part[2], part[3], part[4], 0, part[5])
    end
end

-- Fills the stretch of the cue from `from` to `to` units behind its tip, which widens from `fromWidth` to `toWidth`, grown by `grow` on every side for its outline.
function Pool:cueQuad(dx, dy, start, from, to, fromWidth, toWidth, grow, color)
    local ball, cue = self.cueBall, self.cue
    local nx, ny = -dy, dx
    local ax, ay = ball.x - dx * (start + from - grow), ball.y - dy * (start + from - grow)
    local bx, by = ball.x - dx * (start + to + grow), ball.y - dy * (start + to + grow)
    local a, b = fromWidth / 2 + grow, toWidth / 2 + grow
    cue[1][1], cue[1][2], cue[2][1], cue[2][2] = ax + nx * a, ay + ny * a, bx + nx * b, by + ny * b
    cue[3][1], cue[3][2], cue[4][1], cue[4][2] = bx - nx * b, by - ny * b, ax - nx * a, ay - ny * a
    graphics2d.drawPolygon(cue, color, self.cueOrder)
end

function Pool:draw(area)
    self:drawTable()
    for _, ball in ipairs(self.balls) do
        if ball.valid then
            self:drawBall(ball)
        end
    end
    if not self:isReady() then
        return
    end

    -- A circle cast along the aim finds the first ball or cushion the cue ball would touch, where the guide draws its ghost.
    local cue = self.cueBall
    local dx, dy = math.cos(self.aim), math.sin(self.aim)
    local hit = self.world:castCircle(cue.x, cue.y, Pool.radius, dx * 1600, dy * 1600, {mask = Pool.cushionCategory | Pool.ballCategory})
    local travel = hit and hit.fraction * 1600 or 1600
    local x, y = cue.x + dx * travel, cue.y + dy * travel
    Look.dashed(cue.x + dx * Pool.radius, cue.y + dy * Pool.radius, x - dx * Pool.radius, y - dy * Pool.radius, '#CCFFFFFF', 3)
    graphics2d.drawRing(x, y, Pool.radius, 3 * Look.pixel, '#E6FFFFFF', {layer = 3})
    self:drawCue(dx, dy)
end

return Pool
