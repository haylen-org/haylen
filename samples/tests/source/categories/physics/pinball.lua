-- A pinball table in its wooden cabinet: flippers on revolute joints with motors and limits, mushroom bumpers that kick the ball and light up when it touches them, rubber kickers, a plunger on a spring, steel rails from a chain and a one-way gate at the top of the launch lane, with the score on a board beside the table.
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local input = require('haylen.input')
local physics2d = require('haylen.physics2d')
local ui = require('haylen.ui')

local Look = require('categories.physics.look')
local PhysicsTest = require('categories.physics.physics-test')
local Scenery = require('categories.physics.scenery')

local Pinball = haylen.class('Pinball', PhysicsTest)

Pinball.code = [[
local flipper = world:createJoint('revolute', cabinet, paddle, {ax = -125, ay = 330, enableLimit = true, lower = -0.95, upper = 0, enableMotor = true, maxMotorTorque = 5e6})
flipper.motorSpeed = input.down('leftFlipper') and -20 or 12
local plunger = world:createJoint('prismatic', cabinet, rod, {ax = 315, ay = 350, axisY = 1, enableLimit = true, upper = 70, enableSpring = true, hertz = 3.6, motorSpeed = 160})
plunger.enableMotor = input.down('plunger')  -- The spring launches the ball on release.
gate:addBox(62, 8, {oneWay = {0, -1}})
world.onContactBegin = function(a, b) if b.data.bumper then a:applyImpulse(awayX * a.mass * 520, awayY * a.mass * 520) end end]]

Pinball.actions = {
    {name = 'leftFlipper', type = 'button', bindings = {'key:a', 'key:left', 'key:leftShift', 'button:leftShoulder', 'axis:leftTrigger+', 'virtual:leftFlipper'}},
    {name = 'rightFlipper', type = 'button', bindings = {'key:d', 'key:right', 'key:rightShift', 'button:rightShoulder', 'axis:rightTrigger+', 'virtual:rightFlipper'}},
    {name = 'plunger', type = 'button', bindings = {'key:space', 'key:s', 'key:down', 'button:south', 'virtual:plunger'}},
}

Pinball.ballRadius = 14
Pinball.lane = 315
Pinball.plungerY = 350
Pinball.flippers = {{x = -125, side = 1, action = 'leftFlipper'}, {x = 125, side = -1, action = 'rightFlipper'}}
Pinball.flipperY = 330
Pinball.flipperLength = 105
Pinball.bumpers = {{-100, -170}, {90, -190}, {0, -60}}
Pinball.kick = 520
Pinball.litTime = 0.18
Pinball.pullTime = 0.7
Pinball.kickers = {{{-250, 150}, {-250, 230}, {-180, 268}}, {{180, 268}, {250, 230}, {250, 150}}}
-- Lamps set into the playfield, which blink between their lit and their dim color.
Pinball.lamps = {
    {-200, -40, '#FFFF7FB0', '#FF6A3F6E'},
    {200, -60, '#FF7BC74D', '#FF3A5F4A'},
    {-60, 80, '#FFFFC93C', '#FF6B5B3A'},
    {60, 80, '#FFFFC93C', '#FF6B5B3A'},
    {0, 160, '#FF2FB3C4', '#FF27566E'},
    {-180, -260, '#FF9A6BE0', '#FF43397A'},
    {180, -270, '#FF9A6BE0', '#FF43397A'},
}

function Pinball:enter()
    self:frame{
        code = Pinball.code,
        hint = 'Hold Space, S or the south button to pull the plunger and let go to launch. The flippers take A and D, the arrows, Shift, the shoulders or the triggers. R or the X button starts a new game.',
        controls = {
            ui.button{id = 'launch', text = 'Launch the ball', onClick = function() self.autoPull = Pinball.pullTime end},
            ui.button{id = 'reset', text = 'New game', onClick = function() self:build() end},
        },
        play = true,
        pointer = false,
        actions = Pinball.actions,
        view = {1400, 960},
        overlay = {
            ui.touchButton{action = 'leftFlipper', text = 'Left', size = 150, touchOnly = true, anchor = 'bottomLeft', anchorTo = 'parent', margin = {0, 0, 24, 24}},
            ui.touchButton{action = 'rightFlipper', text = 'Right', size = 150, touchOnly = true, anchor = 'bottomRight', anchorTo = 'parent', margin = {0, 24, 24, 0}},
            ui.touchButton{action = 'plunger', text = 'Plunger', size = 120, touchOnly = true, anchor = 'bottomRight', anchorTo = 'parent', margin = {0, 24, 204, 0}},
        },
    }
    self.camera.position = {25, 0}
    self.best = 0
    self.glow = Scenery.glow()
    self:build()
    self.autoPull = Pinball.pullTime
end

function Pinball:build()
    self.world = physics2d.newWorld({gravity = {0, 700}})
    self.score, self.balls, self.autoPull = 0, 1, 0

    -- The outer wall is one chain that runs counter-clockwise on screen, so it holds the ball on its inner side.
    self.cabinet = self.world:createBody({type = 'static'})
    local wall = {{340, 430}, {340, -280}}
    for step = 1, 15 do
        local angle = math.pi * step / 16
        wall[#wall + 1] = {25 + 315 * math.cos(angle), -280 - 140 * math.sin(angle)}
    end
    for _, point in ipairs({{-290, -280}, {-290, 220}, {-125, 318}}) do
        wall[#wall + 1] = point
    end
    self.cabinet:addChain(wall, false, {friction = 0.1})
    self.cabinet:addSegment(290, -150, 290, 430)
    self.cabinet:addSegment(125, 318, 290, 220)
    self.cabinet:addSegment(290, 430, 340, 430)
    self.wall = {}
    for _, point in ipairs(wall) do
        self.wall[#self.wall + 1] = point[1]
        self.wall[#self.wall + 1] = point[2]
    end
    self.playfield = {}
    for index = 2, #wall do
        self.playfield[#self.playfield + 1] = wall[index]
    end
    self.playfield[#self.playfield + 1] = {125, 318}
    self.playfield[#self.playfield + 1] = {290, 220}
    self.playfield[#self.playfield + 1] = {290, 430}
    self.playfield[#self.playfield + 1] = {340, 430}

    self.gate = self.world:createBody({type = 'static', x = Pinball.lane, y = -168, rotation = -0.61})
    self.gate:addBox(62, 8, {oneWay = {0, -1}})
    Look.dress(self.gate, 'yellow')

    -- The kickers above the flippers bounce the ball back faster than it came, with a restitution above 1.
    self.kickers = {}
    for _, points in ipairs(Pinball.kickers) do
        local kicker = self.world:createBody({type = 'static'})
        kicker:addPolygon(points, {restitution = 1.1})
        Look.dress(kicker, 'teal')
        kicker.data.kicker = true
        self.kickers[#self.kickers + 1] = kicker
    end

    self.bumpers = {}
    for _, spot in ipairs(Pinball.bumpers) do
        local bumper = self.world:createBody({type = 'static', x = spot[1], y = spot[2]})
        bumper:addCircle(28, {restitution = 0.5})
        bumper.data = {bumper = true}
        self.bumpers[#self.bumpers + 1] = {body = bumper, lit = 0}
    end

    -- Each flipper rests against one limit, and its motor swings it to the other while its button is held.
    self.flippers = {}
    for _, spec in ipairs(Pinball.flippers) do
        local flipper = self.world:createBody({x = spec.x, y = Pinball.flipperY, rotation = 0.5 * spec.side})
        local length = Pinball.flipperLength * spec.side
        local outline = {{0, -12}, {length, -6}, {length, 6}, {0, 12}}
        flipper:addPolygon(outline, {density = 2, friction = 0.4})
        flipper:addCircle(12, {density = 2})
        Look.dress(flipper, 'yellow')
        local lower, upper = spec.side > 0 and -0.95 or 0, spec.side > 0 and 0 or 0.95
        local joint = self.world:createJoint('revolute', self.cabinet, flipper, {ax = spec.x, ay = Pinball.flipperY, enableLimit = true, lower = lower, upper = upper, enableMotor = true, maxMotorTorque = 5e6})
        self.flippers[#self.flippers + 1] = {body = flipper, joint = joint, side = spec.side, action = spec.action}
    end

    self.plunger = self.world:createBody({x = Pinball.lane, y = Pinball.plungerY})
    self.plunger:addBox(44, 24, {density = 4})
    Look.dress(self.plunger, 'steel')
    self.spring = self.world:createJoint('prismatic', self.cabinet, self.plunger, {ax = Pinball.lane, ay = Pinball.plungerY, axisX = 0, axisY = 1, enableLimit = true, lower = -10, upper = 70, enableSpring = true, hertz = 3.6, dampingRatio = 0.05, maxMotorForce = 1e5, motorSpeed = 160})
    self:serve()

    self.world.onContactBegin = function(a, b, contact)
        local ball, other = a, b
        if b.data and b.data.ball then
            ball, other = b, a
        end
        if not (ball.data and ball.data.ball) then
            return
        end
        if other.data and other.data.bumper then
            local dx, dy = ball.x - other.x, ball.y - other.y
            local length = math.max(1, math.sqrt(dx * dx + dy * dy))
            ball:applyImpulse(dx / length * ball.mass * Pinball.kick, dy / length * ball.mass * Pinball.kick)
            self.score = self.score + 100
            for _, bumper in ipairs(self.bumpers) do
                if bumper.body == other then
                    bumper.lit = Pinball.litTime
                end
            end
        elseif other.data and other.data.kicker then
            self.score = self.score + 10
        end
    end
end

function Pinball:serve()
    self.ball = self.world:createBody({x = Pinball.lane, y = Pinball.plungerY - 12 - Pinball.ballRadius, bullet = true})
    self.ball:addCircle(Pinball.ballRadius, {density = 2, restitution = 0.3, friction = 0.2})
    Look.dress(self.ball, 'steel')
    self.ball.data.ball = true
end

function Pinball:exit()
    Pinball.super.exit(self)
    input.clearVirtual()
end

function Pinball:update(dt)
    Pinball.super.update(self, dt)
    if input.pressed('reset') then
        self:build()
    end
    for _, bumper in ipairs(self.bumpers) do
        bumper.lit = math.max(0, bumper.lit - dt)
    end
    self.best = math.max(self.best, self.score)
    local pulled = math.max(0, self.spring.translation)
    if pulled > 2 then
        self:status(string.format('The plunger is pulled back %d units and launches the ball when it goes.', math.floor(pulled + 0.5)))
    else
        self:status(string.format('Ball %d rolls at %d units per second.', self.balls, math.floor(self.ball.velocity:length() + 0.5)))
    end
end

function Pinball:fixedUpdate(step)
    for _, flipper in ipairs(self.flippers) do
        flipper.joint.motorSpeed = (input.down(flipper.action) and -20 or 12) * flipper.side
    end
    self.autoPull = math.max(0, self.autoPull - step)
    self.spring.enableMotor = input.down('plunger') or self.autoPull > 0
    self:simulate(self.world, step)
    if self.ball.y > 460 then
        self.ball:destroy()
        self.balls = self.balls + 1
        self:serve()
    end
end

-- The cabinet is a wooden frame around a deep blue playfield with lamps, its rails are steel and the apron at the bottom hides the drain.
function Pinball:drawTable()
    local bounds = graphics2d.canvasBounds()
    graphics2d.drawRect(bounds, '#FF221D3B', {layer = -30})
    graphics2d.draw(self.glow, 25, 0, {width = 1400, height = 1300, color = '#405A4AA8', layer = -29})
    local unit = Look.pixel
    graphics2d.drawShape({-340, -470, 730, 960}, {radius = 70, color = '#FFA86B3C', borderWidth = 5 * unit, borderColor = Look.ink, layer = -20})
    graphics2d.drawShape({-322, -452, 694, 924}, {radius = 58, color = '#FF7A4724', layer = -20})
    graphics2d.drawPolygon(self.playfield, '#FF1F2F66', {layer = -19})
    local time = haylen.elapsed()
    for index, lamp in ipairs(Pinball.lamps) do
        local on = math.sin(time * 4 + index) > 0
        graphics2d.drawCircle(lamp[1], lamp[2], 13, on and lamp[3] or lamp[4], {layer = -18})
        graphics2d.drawRing(lamp[1], lamp[2], 13, 3 * unit, Look.ink, {layer = -18})
        if on then
            graphics2d.draw(self.glow, lamp[1], lamp[2], {width = 70, height = 70, color = lamp[3], blend = 'additive', layer = -18})
        end
    end
    graphics2d.drawStroke(self.wall, {width = 14 + 7 * unit, color = Look.ink, join = 'round', cap = 'round', layer = -10})
    graphics2d.drawStroke(self.wall, {width = 14, color = '#FFC9D3E2', join = 'round', cap = 'round', layer = -10})
    for _, rail in ipairs({{290, -150, 290, 430}, {125, 318, 290, 220}}) do
        Look.line(rail[1], rail[2], rail[3], rail[4], 10, '#FFC9D3E2', -10)
    end
    graphics2d.drawShape({-300, 400, 650, 70}, {radius = 14, color = '#FF2F3352', borderWidth = 4 * unit, borderColor = Look.ink, layer = 5})
    graphics2d.drawShape({-60, 386, 120, 40}, {radius = 20, color = '#FF15172A', layer = 5})
end

-- A bumper is a red mushroom with a white rim that flashes and glows when the ball touches it.
function Pinball:drawBumpers()
    local unit = Look.pixel
    for _, bumper in ipairs(self.bumpers) do
        local body = bumper.body
        local lit = bumper.lit / Pinball.litTime
        if lit > 0 then
            graphics2d.draw(self.glow, body.x, body.y, {width = 200, height = 200, color = {1, 0.85, 0.4, lit}, blend = 'additive', layer = 1})
        end
        graphics2d.drawShape({body.x - 36, body.y - 36, 72, 72}, {kind = 'ellipse', color = lit > 0 and '#FFFFF4C0' or '#FFF2F3FF', borderWidth = 4 * unit, borderColor = Look.ink, layer = 1})
        Look.disc(body.x, body.y, 26, 'red', 1)
        graphics2d.drawCircle(body.x, body.y, 9, lit > 0 and '#FFFFC93C' or '#FFFFD9A3', {layer = 1})
    end
end

-- The score board stands beside the table like the back box of a cabinet, with the score in large digits over the best score and the ball in play.
function Pinball:drawBoard()
    local unit = Look.pixel
    graphics2d.drawShape({-780, -250, 320, 250}, {radius = 26, color = '#FFA86B3C', borderWidth = 5 * unit, borderColor = Look.ink, layer = -20})
    graphics2d.drawShape({-762, -232, 284, 214}, {radius = 16, color = '#FF15172A', layer = -20})
    self:tag('Score', -620, -196, {size = 22, color = '#FFFFC93C'})
    self:tag(tostring(self.score), -620, -138, {size = 56})
    self:tag(string.format('Ball %d    Best %d', self.balls, self.best), -620, -62, {size = 22, color = '#FFC9D3E2'})
end

function Pinball:draw(area)
    self:drawTable()
    Look.drawAll(self.kickers)
    Look.draw(self.gate)
    self:drawBumpers()
    for _, flipper in ipairs(self.flippers) do
        Look.draw(flipper.body, {layer = 2})
    end
    local plunger = self.plunger
    Look.spring(Pinball.lane, plunger.y + 12, Pinball.lane, 432, 24, 5, '#FFC9D3E2', 1)
    Look.draw(plunger, {layer = 2})
    Look.draw(self.ball, {layer = 3})
    self:drawBoard()
    self:label('One-way gate', 470, -168, {size = 17})
end

return Pinball
