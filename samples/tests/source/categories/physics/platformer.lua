-- A platformer character made with `physics2d.newMover`: it runs with acceleration, jumps higher while the button is held, jumps a moment after leaving a ledge and a moment before landing, walks up slopes and steps, jumps up through one-way platforms and drops down through them, rides a moving platform and pushes crates, collecting coins on the way to the flag while the camera follows it.
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local input = require('haylen.input')
local m = require('haylen.math')
local physics2d = require('haylen.physics2d')
local ui = require('haylen.ui')

local parts = require('categories.physics.parts')
local PhysicsTest = require('categories.physics.physics-test')

local Platformer = haylen.class('Platformer', PhysicsTest)

Platformer.actions = {
    {name = 'move', type = 'vector', up = {'key:w', 'key:up', 'button:dpadUp'}, down = {'key:s', 'key:down', 'button:dpadDown'}, left = {'key:a', 'key:left', 'button:dpadLeft'}, right = {'key:d', 'key:right', 'button:dpadRight'}, bindings = {'stick:left', 'virtualStick:move'}},
    {name = 'jump', type = 'button', bindings = {'key:space', 'key:k', 'button:south', 'virtual:jump'}},
}

local kGravity = 2400
local kRunSpeed, kAcceleration, kAirAcceleration = 420, 3200, 1600
local kJumpSpeed, kCutSpeed = 900, 300
local kCoyoteTime, kBufferTime = 0.1, 0.12
local kRadius, kHeight = 16, 60
local kGround = {{-200, -800}, {-200, 400}, {600, 400}, {900, 250}, {1150, 250}, {1150, 270}, {1190, 270}, {1190, 290}, {1230, 290}, {1230, 310}, {1270, 310}, {1270, 400}, {1900, 400}, {1900, 520}, {2300, 520}, {2300, 400}, {3100, 400}, {3100, -800}}
local kLedges = {{300, 260, 200}, {430, 120, 160}, {1500, 240, 220}, {1650, 100, 180}, {2700, 250, 200}}
local kCoins = {{300, 220}, {430, 80}, {760, 280}, {1210, 220}, {1500, 200}, {1650, 60}, {2100, 380}, {2450, 300}, {2700, 210}, {2900, 340}}
local kFlag = {3000, 400}

function Platformer:enter()
    self:frame{
        hint = 'Run with A and D, the arrows, the left stick or the touch stick, and jump with Space, K or the south button. Hold the jump to jump higher, and hold down while jumping to drop through a platform. R or the X button starts over.',
        controls = {
            ui.checkbox{id = 'assists', text = 'Jump before landing and after leaving a ledge', checked = true, onChange = function(event) self.assists = event.checked end},
            ui.button{id = 'reset', text = 'Start over', onClick = function() self:build() end},
        },
        play = true,
        pointer = false,
        actions = Platformer.actions,
        overlay = {
            ui.touchStick{action = 'move', radius = 110, mode = 'floating', touchOnly = true, anchor = 'bottomLeft', margin = {0, 0, 110, 40}, width = 360, height = 300},
            ui.touchButton{action = 'jump', text = 'Jump', size = 150, touchOnly = true, anchor = 'bottomRight', margin = {0, 540, 110, 0}},
        },
    }
    self.assists = true
    self.camera.positionSmoothing = true
    self.camera.positionSmoothingSpeed = 6
    self:build()
end

function Platformer:build()
    self.world = physics2d.newWorld({gravity = {0, kGravity}})
    self.ground = self.world:createBody({type = 'static'})
    parts.chain(self.ground, kGround, false, {friction = 0.8})
    local fill = {}
    for index = 2, #kGround - 1 do
        fill[#fill + 1] = kGround[index]
    end
    fill[#fill + 1] = {3100, 900}
    fill[#fill + 1] = {-200, 900}
    parts.outline(self.ground, fill)
    parts.paint(self.ground, '#FF4E5D45')

    self.ledges = {}
    for _, spot in ipairs(kLedges) do
        local ledge = self.world:createBody({type = 'static', x = spot[1], y = spot[2]})
        parts.box(ledge, spot[3], 16, {oneWay = {0, -1}})
        parts.paint(ledge, '#FF8D6E63')
        self.ledges[#self.ledges + 1] = ledge
    end
    self.lift = self.world:createBody({type = 'kinematic', x = 2100, y = 420})
    parts.box(self.lift, 180, 16, {friction = 0.9})
    parts.paint(self.lift, '#FFFFB74D')

    self.crates = {}
    for index = 0, 2 do
        local crate = self.world:createBody({x = 1700 + index * 50, y = 350 - index * 5})
        parts.box(crate, 46, 46, {friction = 0.6})
        self.crates[#self.crates + 1] = crate
    end

    self.coins = {}
    for _, spot in ipairs(kCoins) do
        self.coins[#self.coins + 1] = {x = spot[1], y = spot[2], taken = false}
    end
    self.collected, self.finished, self.time = 0, false, 0

    self.hero = physics2d.newMover(self.world, {x = 0, y = 340, radius = kRadius, height = kHeight, stepHeight = 22, snapDistance = 16, maxSlope = math.rad(50)})
    self.velocity = m.vec2(0, 0)
    self.coyote, self.buffer, self.jumping = 0, 0, false
    self.camera:snapTo(0, 200)
end

function Platformer:exit()
    Platformer.super.exit(self)
    input.clearVirtual()
end

function Platformer:update(dt)
    Platformer.super.update(self, dt)
    if input.pressed('reset') then
        self:build()
    end
    if input.pressed('jump') then
        self.buffer = kBufferTime
    end
    local hero = self.hero
    self.camera:follow(hero.x + self.velocity.x * 0.2, hero.y - 80, dt)
    self:status(string.format('Coins %d of %d%s, speed %.0f, %s, step %.2f ms', self.collected, #self.coins, self.finished and string.format(', flag reached in %.1f seconds', self.time) or '', math.abs(self.velocity.x), hero.grounded and 'on the ground' or 'in the air', self:stepTime()))
end

function Platformer:onLedge()
    local ground = self.hero.groundBody
    for _, ledge in ipairs(self.ledges) do
        if ledge == ground then
            return true
        end
    end
    return false
end

function Platformer:approach(value, target, rate)
    if value < target then
        return math.min(value + rate, target)
    end
    return math.max(value - rate, target)
end

function Platformer:fixedUpdate(step)
    local hero = self.hero
    local moveX, moveY = input.vector('move')
    local grounded = hero.grounded
    self.coyote = grounded and kCoyoteTime or math.max(0, self.coyote - step)
    self.buffer = math.max(0, self.buffer - step)
    if not self.finished then
        self.time = self.time + step
    end

    -- The run eases toward the speed of the stick, faster on the ground than in the air.
    local velocity = hero:clip(self.velocity.x, self.velocity.y + kGravity * step)
    velocity.x = self:approach(velocity.x, moveX * kRunSpeed, (grounded and kAcceleration or kAirAcceleration) * step)

    -- A press counts for a moment before landing, and a ledge still counts for a moment after it.
    local canJump = grounded or self.assists and self.coyote > 0
    local wantsJump = self.buffer > 0 and (self.assists or input.pressed('jump'))
    if wantsJump and canJump then
        self.buffer, self.coyote = 0, 0
        if moveY > 0.5 and grounded and self:onLedge() then
            hero:dropThrough()
        else
            velocity.y = -kJumpSpeed + math.min(0, hero.groundVelocity.y)
            self.jumping = true
        end
    end
    if self.jumping and not input.down('jump') and velocity.y < -kCutSpeed then
        velocity.y = -kCutSpeed
    end
    if velocity.y >= 0 then
        self.jumping = false
    end

    hero:move(velocity.x * step, velocity.y * step)
    self.velocity = hero:clip(velocity.x, velocity.y)

    self.lift:moveTo(2100, 420 - (math.sin(self.time * 0.9) * 0.5 + 0.5) * 300)
    self:simulate(self.world, step)

    for _, coin in ipairs(self.coins) do
        if not coin.taken and math.abs(coin.x - hero.x) < 30 and math.abs(coin.y - hero.y) < 44 then
            coin.taken = true
            self.collected = self.collected + 1
        end
    end
    if math.abs(hero.x - kFlag[1]) < 40 then
        self.finished = true
    end
    if hero.y > 900 then
        hero.position = {0, 340}
        self.velocity = m.vec2(0, 0)
    end
end

function Platformer:draw(area)
    parts.draw(self.ground)
    parts.drawAll(self.ledges)
    parts.draw(self.lift)
    parts.drawAll(self.crates, {layer = 1})
    for _, coin in ipairs(self.coins) do
        if not coin.taken then
            graphics2d.drawCircle(coin.x, coin.y, 14, '#FFFFD54F', {layer = 1})
            graphics2d.drawCircle(coin.x, coin.y, 7, '#FFFFB300', {layer = 1})
        end
    end
    graphics2d.drawLine(kFlag[1], kFlag[2], kFlag[1], kFlag[2] - 160, 6, '#FFECEFF1', {layer = 1})
    graphics2d.drawPolygon({{kFlag[1], kFlag[2] - 160}, {kFlag[1] + 80, kFlag[2] - 130}, {kFlag[1], kFlag[2] - 100}}, self.finished and '#FF6FDCA0' or '#FFFF8A84', {layer = 1})

    local hero = self.hero
    local reach = kHeight / 2 - kRadius
    local color = hero.grounded and '#FF4DD0E1' or '#FF80DEEA'
    graphics2d.drawLine(hero.x, hero.y - reach, hero.x, hero.y + reach, kRadius * 2, color, {layer = 2})
    graphics2d.drawCircle(hero.x, hero.y - reach, kRadius, color, {layer = 2})
    graphics2d.drawCircle(hero.x, hero.y + reach, kRadius, color, {layer = 2})
    local facing = self.velocity.x < -1 and -1 or 1
    graphics2d.drawCircle(hero.x + facing * 7, hero.y - reach - 3, 4, '#FF263238', {layer = 3})
end

return Platformer
