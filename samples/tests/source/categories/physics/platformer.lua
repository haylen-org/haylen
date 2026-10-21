-- A platformer hero made with `physics2d.newMover`: it runs with acceleration, jumps higher while the button is held, jumps a moment after leaving a ledge and a moment before landing, walks up slopes and steps, jumps up through one-way platforms and drops down through them, rides a moving platform and pushes crates, collecting coins on the way to the flag while the camera follows it inside the limits of the level.
local assets = require('haylen.assets')
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local input = require('haylen.input')
local m = require('haylen.math')
local physics2d = require('haylen.physics2d')
local ui = require('haylen.ui')

local Burst = require('categories.physics.burst')
local Hero = require('categories.physics.hero')
local Look = require('categories.physics.look')
local PhysicsTest = require('categories.physics.physics-test')
local Scenery = require('categories.physics.scenery')

local Platformer = haylen.class('Platformer', PhysicsTest)

Platformer.code = [[
local hero = physics2d.newMover(world, {x = 0, y = 330, radius = 18, height = 76, stepHeight = 22, snapDistance = 16, maxSlope = math.rad(50)})
local velocity = hero:clip(vx, vy + 2400 * step)
if jumpPressed and (hero.grounded or coyote > 0) then velocity.y = -900 end
hero:move(velocity.x * step, velocity.y * step)
velocity = hero:clip(velocity.x, velocity.y)
camera.limits = {-200, -800, 3300, 1400}  -- The view never leaves the level.]]

Platformer.actions = {
    {name = 'move', type = 'vector', up = {'key:w', 'key:up', 'button:dpadUp'}, down = {'key:s', 'key:down', 'button:dpadDown'}, left = {'key:a', 'key:left', 'button:dpadLeft'}, right = {'key:d', 'key:right', 'button:dpadRight'}, bindings = {'stick:left', 'virtualStick:move'}},
    {name = 'jump', type = 'button', bindings = {'key:space', 'key:k', 'button:south', 'virtual:jump'}},
}

Platformer.gravity = 2400
Platformer.runSpeed, Platformer.acceleration, Platformer.airAcceleration = 420, 3200, 1600
Platformer.jumpSpeed, Platformer.cutSpeed = 900, 300
Platformer.coyoteTime, Platformer.bufferTime = 0.1, 0.12
Platformer.radius, Platformer.height = 18, 76
Platformer.outline = {{-200, -800}, {-200, 400}, {600, 400}, {900, 250}, {1150, 250}, {1150, 270}, {1190, 270}, {1190, 290}, {1230, 290}, {1230, 310}, {1270, 310}, {1270, 400}, {1900, 400}, {1900, 520}, {2300, 520}, {2300, 400}, {3100, 400}, {3100, -800}}
-- The grassy stretches of the ground, which the stone steps and the walls of the pit break.
Platformer.lawns = {{{-200, 400}, {600, 400}, {900, 250}, {1150, 250}}, {{1270, 400}, {1900, 400}}, {{1900, 520}, {2300, 520}}, {{2300, 400}, {3100, 400}}}
Platformer.steps = {{1150, 270}, {1190, 290}, {1230, 310}}
Platformer.ledges = {{300, 260, 200}, {430, 120, 160}, {1500, 240, 220}, {1650, 100, 180}, {2700, 250, 200}}
Platformer.coins = {{300, 220}, {430, 80}, {760, 280}, {1210, 220}, {1500, 200}, {1650, 60}, {2100, 380}, {2450, 300}, {2700, 210}, {2900, 340}}
Platformer.flag = {3000, 400}
Platformer.limits = {-200, -800, 3300, 1400}

function Platformer:enter()
    self:frame{
        code = Platformer.code,
        hint = 'Run with A and D, the arrows or the left stick, and jump with Space, K or the south button. Hold the jump to jump higher, and hold down while jumping to drop through a platform. R or the X button starts over.',
        controls = {
            ui.checkbox{id = 'assists', text = 'Assisted jumps', checked = true, onChange = function(event) self.assists = event.checked end},
            ui.label{text = 'A jump counts a moment before landing and a moment after leaving a ledge.', font = 'caption', color = 'textMuted'},
            ui.button{id = 'reset', text = 'Start over', onClick = function() self:build() end},
        },
        play = true,
        pointer = false,
        actions = Platformer.actions,
        view = {1300, 620},
        overlay = {
            ui.touchStick{action = 'move', radius = 110, mode = 'floating', touchOnly = true, anchor = 'bottomLeft', anchorTo = 'parent', margin = {0, 0, 24, 24}, width = 360, height = 300},
            ui.touchButton{action = 'jump', text = 'Jump', size = 150, touchOnly = true, anchor = 'bottomRight', anchorTo = 'parent', margin = {0, 24, 24, 0}},
        },
    }
    self.assists = true
    self.camera.positionSmoothing = true
    self.camera.positionSmoothingSpeed = 6
    self.camera.limits = Platformer.limits
    self.heroArt = Hero(Platformer.height)
    self.coin = assets.texture('physics/coin.png', {filter = 'linear'})
    self.flag = assets.texture('physics/flag.png', {filter = 'linear'})
    self.pickup = Burst('pickups/coin-collect', {count = 3, scale = 0.6, layer = 30})
    self.confetti = Burst('celebration/confetti-pop', {count = 1, scale = 1, layer = 30})
    self.lawns = {}
    for index, lawn in ipairs(Platformer.lawns) do
        self.lawns[index] = Scenery.newTerrain(lawn, 1400)
    end
    local fill = {}
    for index = 2, #Platformer.outline - 1 do
        fill[#fill + 1] = Platformer.outline[index]
    end
    fill[#fill + 1] = {3100, 1400}
    fill[#fill + 1] = {-200, 1400}
    self.fill = fill
    self.dirt = {layer = -10}
    self:build()
end

function Platformer:build()
    self.world = physics2d.newWorld({gravity = {0, Platformer.gravity}})
    self.ground = self.world:createBody({type = 'static'})
    self.ground:addChain(Platformer.outline, false, {friction = 0.8})

    self.ledges = {}
    for _, spot in ipairs(Platformer.ledges) do
        local ledge = self.world:createBody({type = 'static', x = spot[1], y = spot[2]})
        ledge:addBox(spot[3], 20, {oneWay = {0, -1}})
        Look.dress(ledge, 'wood')
        self.ledges[#self.ledges + 1] = ledge
    end
    self.lift = self.world:createBody({type = 'kinematic', x = 2100, y = 420})
    self.lift:addBox(180, 20, {friction = 0.9})
    Look.dress(self.lift, 'steel')

    self.crates = {}
    for index = 0, 2 do
        local crate = self.world:createBody({x = 1700 + index * 52, y = 350 - index * 5})
        crate:addBox(48, 48, {friction = 0.6})
        Look.dress(crate, 'woodCrate')
        self.crates[#self.crates + 1] = crate
    end

    self.coins = {}
    for _, spot in ipairs(Platformer.coins) do
        self.coins[#self.coins + 1] = {x = spot[1], y = spot[2], taken = false}
    end
    self.collected, self.finished, self.time = 0, false, 0

    self.hero = physics2d.newMover(self.world, {x = 0, y = 400 - Platformer.height / 2 - 2, radius = Platformer.radius, height = Platformer.height, stepHeight = 22, snapDistance = 16, maxSlope = math.rad(50)})
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
        self.buffer = Platformer.bufferTime
    end
    local hero = self.hero
    self.camera:follow(hero.x + self.velocity.x * 0.2, hero.y - 80, dt)
    self.heroArt:update(dt, self.velocity.x, hero.grounded)
    self.pickup:update(dt)
    self.confetti:update(dt)
    local where = hero.grounded and (math.abs(self.velocity.x) > 30 and 'runs' or 'stands') or 'is in the air'
    if self.finished then
        self:status(string.format('The hero reached the flag in %.1f seconds with %d of %d coins.', self.time, self.collected, #self.coins))
    else
        self:status(string.format('The hero %s with %d of %d coins on the way to the flag.', where, self.collected, #self.coins))
    end
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

function Platformer.approach(value, target, rate)
    if value < target then
        return math.min(value + rate, target)
    end
    return math.max(value - rate, target)
end

function Platformer:fixedUpdate(step)
    local hero = self.hero
    local moveX, moveY = input.vector('move')
    local grounded = hero.grounded
    self.coyote = grounded and Platformer.coyoteTime or math.max(0, self.coyote - step)
    self.buffer = math.max(0, self.buffer - step)
    if not self.finished then
        self.time = self.time + step
    end

    -- The run eases toward the speed of the stick, faster on the ground than in the air.
    local velocity = hero:clip(self.velocity.x, self.velocity.y + Platformer.gravity * step)
    velocity.x = Platformer.approach(velocity.x, moveX * Platformer.runSpeed, (grounded and Platformer.acceleration or Platformer.airAcceleration) * step)

    -- A press counts for a moment before landing, and a ledge still counts for a moment after it.
    local canJump = grounded or self.assists and self.coyote > 0
    local wantsJump = self.buffer > 0 and (self.assists or input.pressed('jump'))
    if wantsJump and canJump then
        self.buffer, self.coyote = 0, 0
        if moveY > 0.5 and grounded and self:onLedge() then
            hero:dropThrough()
        else
            velocity.y = -Platformer.jumpSpeed + math.min(0, hero.groundVelocity.y)
            self.jumping = true
        end
    end
    if self.jumping and not input.down('jump') and velocity.y < -Platformer.cutSpeed then
        velocity.y = -Platformer.cutSpeed
    end
    if velocity.y >= 0 then
        self.jumping = false
    end

    hero:move(velocity.x * step, velocity.y * step)
    self.velocity = hero:clip(velocity.x, velocity.y)

    self.lift:moveTo(2100, 420 - (math.sin(self.time * 0.9) * 0.5 + 0.5) * 300)
    self:simulate(self.world, step)

    for _, coin in ipairs(self.coins) do
        if not coin.taken and math.abs(coin.x - hero.x) < 34 and math.abs(coin.y - hero.y) < 50 then
            coin.taken = true
            self.collected = self.collected + 1
            self.pickup:fire(coin.x, coin.y)
        end
    end
    if not self.finished and math.abs(hero.x - Platformer.flag[1]) < 40 then
        self.finished = true
        self.confetti:fire(Platformer.flag[1], Platformer.flag[2] - 120)
    end
    if hero.y > 900 then
        hero.position = {0, 330}
        self.velocity = m.vec2(0, 0)
    end
end

function Platformer:drawLevel()
    graphics2d.drawPolygon(self.fill, Scenery.dirt, self.dirt)
    for _, lawn in ipairs(self.lawns) do
        Scenery.drawTerrain(lawn, -10)
    end
    for _, step in ipairs(Platformer.steps) do
        Look.block(step[1] + 20, (step[2] + 400) / 2, 40, 400 - step[2], 'stone', -8)
    end
    for _, x in ipairs({1890, 2310}) do
        Look.block(x, 460, 20, 120, 'stone', -8)
    end
    for _, x in ipairs({-230, 3130}) do
        Look.block(x, -200, 60, 1200, 'stone', -8)
    end
end

-- Coins turn on their edge, so they read as coins and not as dots.
function Platformer:drawCoins()
    local turn = math.cos(haylen.elapsed() * 4)
    for _, coin in ipairs(self.coins) do
        if not coin.taken then
            graphics2d.draw(self.coin, coin.x, coin.y, {width = 40 * math.max(0.15, math.abs(turn)), height = 40, layer = 1})
        end
    end
end

function Platformer:draw(area)
    Scenery.sky(self.camera, 400, {parallax = 1})
    self:drawLevel()
    Look.drawAll(self.ledges)
    Look.draw(self.lift)
    Look.drawAll(self.crates, {layer = 1})
    self:drawCoins()
    graphics2d.draw(self.flag, Platformer.flag[1], Platformer.flag[2], {width = 102, height = 144, pivotX = 0.16, pivotY = 0.97, layer = 1})
    self.heroArt:draw(self.hero.x, self.hero.y + Platformer.height / 2, 2)
    self.pickup:draw()
    self.confetti:draw()
end

return Platformer
