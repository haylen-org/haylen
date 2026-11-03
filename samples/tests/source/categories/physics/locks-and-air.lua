-- Three capabilities of the bodies and fields of a world side by side: motion locks that hold a lift on its line, a sledge at its height and a turnstile on its pin, a gate of a thin sensor that counts shots too fast to stop inside it in any step, and a wind field whose air carries light plates further than heavy balls and makes the tilted ones sail.
local haylen = require('haylen')
local input = require('haylen.input')
local m = require('haylen.math')
local physics2d = require('haylen.physics2d')
local timer = require('haylen.timer')
local ui = require('haylen.ui')

local Grab = require('categories.physics.grab')
local Look = require('categories.physics.look')
local PhysicsTest = require('categories.physics.physics-test')
local Scenery = require('categories.physics.scenery')

local LocksAndAir = haylen.class('LocksAndAir', PhysicsTest)

LocksAndAir.code = [[
local sledge = world:createBody({x = -520, y = 40, lockY = true, lockRotation = true})
local turnstile = world:createBody({x = -390, y = 220, lockX = true, lockY = true})
gate:addBox(6, 320, {sensor = true})  -- Shots that cross it within one step still count.
local shot = world:createBody({x = -170, y = 110, vx = 4800, continuousThreshold = 0.1})
physics2d.newForceField(world, {kind = 'wind', x = 520, y = 60, width = 480, height = 560,
    direction = {1, 0}, strength = 400, lift = 0.6})  -- Air that sails tilted plates.]]

LocksAndAir.ground = 340
LocksAndAir.liftX = -680
LocksAndAir.sledgeY = 40
LocksAndAir.wall = 790
-- The middle stage fires shots from a cannon at the left, through the gate, into a wall of steel.
LocksAndAir.cannon = {-200, 110}
LocksAndAir.gateX = 20
LocksAndAir.target = 200
LocksAndAir.maxShots = 12
-- The wind blows through the right stage, from its left edge to the wall.
LocksAndAir.wind = {x = 520, y = 60, width = 520, height = 560}
LocksAndAir.maxPlates = 16
LocksAndAir.plateColors = {'green', 'yellow', 'orange', 'pink'}

function LocksAndAir:enter()
    self:frame{
        code = LocksAndAir.code,
        hint = 'Drag the lift, the sledge and the turnstile: each moves only the way its locks allow, and without locks they fall. Fire shots through the gate, and drop plates and balls into the wind. R or the X button rebuilds.',
        controls = {
            ui.toggle{id = 'locks', text = 'Motion locks', checked = true, onChange = function(event) self:lock(event.checked) end},
            ui.button{id = 'balls', text = 'Drop balls', onClick = function() self:dropBalls() end},
            ui.toggle{id = 'auto', text = 'Fire shots', checked = true, onChange = function(event) self.firing = event.checked end},
            ui.label{text = 'Wind speed', font = 'caption', color = 'textMuted'},
            ui.slider{id = 'wind', value = 400, min = 0, max = 700, step = 50, showValue = true, decimals = 0, onChange = function(event) self:blow(event.value, self.lift) end},
            ui.label{text = 'Lift', font = 'caption', color = 'textMuted'},
            ui.slider{id = 'lift', value = 0.6, min = 0, max = 2, step = 0.1, showValue = true, decimals = 1, onChange = function(event) self:blow(self.speed, event.value) end},
            ui.button{id = 'plates', text = 'Drop plates', onClick = function() self:dropPlates(6) end},
            ui.button{id = 'reset', text = 'Rebuild', onClick = function() self:build() end},
        },
        focus = 'locks',
        view = {1640, 760},
    }
    self.camera.position = {0, 20}
    self.locked, self.firing, self.speed, self.lift = true, true, 400, 0.6
    self.random = m.random(67)
    self:build()
    timer.every(0.45, function()
        if self.firing then
            self:fire()
        end
    end, {owner = self})
end

function LocksAndAir:build()
    if self.world then
        self.world:destroy()
    end
    self.world = physics2d.newWorld({owner = self})
    self.grab = Grab(self.world)
    self.balls, self.shots, self.plates = {}, {}, {}
    self.fired, self.counted = 0, 0

    local yard = self.world:createBody({type = 'static'})
    yard:addBox(4000, 40, {offsetY = LocksAndAir.ground + 20})
    yard:addBox(40, 900, {offsetX = -LocksAndAir.wall - 20})
    yard:addBox(40, 900, {offsetX = LocksAndAir.wall + 20})
    yard:addBox(40, 900, {offsetX = -260})
    yard:addBox(40, 900, {offsetX = 260})

    -- The left stage: a lift that only rises and falls, a sledge that only slides at its height and a turnstile that only turns.
    self.liftBody = self.world:createBody({x = LocksAndAir.liftX, y = 120})
    self.liftBody:addBox(110, 18)
    Look.dress(self.liftBody, 'steel')
    self.sledge = self.world:createBody({x = -520, y = LocksAndAir.sledgeY})
    self.sledge:addBox(120, 22, {friction = 0.1})
    Look.dress(self.sledge, 'woodCrate')
    self.turnstile = self.world:createBody({x = -390, y = 220})
    self.turnstile:addBox(180, 16)
    self.turnstile:addBox(16, 180)
    Look.dress(self.turnstile, 'gold')
    self:lock(self.locked)
    self:dropBalls()

    -- The middle stage: a gate of a thin sensor between the cannon and its target.
    self.gate = self.world:createBody({type = 'static', x = LocksAndAir.gateX, y = LocksAndAir.cannon[2]})
    self.gate:addBox(6, 320, {sensor = true})
    local target = self.world:createBody({type = 'static', x = LocksAndAir.target, y = LocksAndAir.cannon[2]})
    target:addBox(30, 240)
    Look.dress(target, 'iron')
    self.target = target
    self.world.onSensorBegin = function(sensor, visitor)
        if visitor.data.shot then
            self.counted = self.counted + 1
            self.flash = 1
        end
    end

    -- The right stage: the wind, and plates and balls to blow away.
    local wind = LocksAndAir.wind
    self.field = physics2d.newForceField(self.world, {kind = 'wind', x = wind.x, y = wind.y, width = wind.width, height = wind.height, direction = {1, 0}, strength = self.speed, lift = self.lift})
    self:dropPlates(8)
    for index = 1, 2 do
        local ball = self.world:createBody({x = 340 + index * 90, y = LocksAndAir.ground - 40})
        ball:addCircle(26, {density = 4, restitution = 0.2})
        Look.dress(ball, 'iron', {spin = true})
        self.plates[#self.plates + 1] = ball
    end
end

-- Turns the locks of the three bodies of the left stage on or off, so they hold their lines or fall like any body.
function LocksAndAir:lock(value)
    self.locked = value
    self.liftBody.lockX, self.liftBody.lockRotation = value, value
    self.sledge.lockY, self.sledge.lockRotation = value, value
    self.turnstile.lockX, self.turnstile.lockY = value, value
    for _, body in ipairs({self.liftBody, self.sledge, self.turnstile}) do
        body.awake = true
    end
end

function LocksAndAir:dropBalls()
    for _ = 1, 5 do
        local ball = self.world:createBody({x = self.random:range(-740, -300), y = self.random:range(-360, -200)})
        ball:addCircle(self.random:range(14, 22), {restitution = 0.3})
        Look.dress(ball, 'blue')
        self.balls[#self.balls + 1] = ball
        if #self.balls > 20 then
            table.remove(self.balls, 1):destroy()
        end
    end
end

function LocksAndAir:fire()
    local cannon = LocksAndAir.cannon
    local shot = self.world:createBody({x = cannon[1] + 30, y = cannon[2], vx = 4800, gravityScale = 0, continuousThreshold = 0.1})
    shot:addCircle(6, {density = 4})
    Look.dress(shot, 'yellow')
    shot.data.shot = true
    self.fired = self.fired + 1
    self.shots[#self.shots + 1] = shot
    if #self.shots > LocksAndAir.maxShots then
        table.remove(self.shots, 1):destroy()
    end
end

function LocksAndAir:dropPlates(count)
    local wind = LocksAndAir.wind
    for _ = 1, count do
        local plate = self.world:createBody({x = self.random:range(wind.x - wind.width / 2 + 30, wind.x), y = self.random:range(-300, 0), rotation = self.random:range(-0.6, 0.6)})
        plate:addBox(54, 6, {density = 2.5})
        Look.dress(plate, LocksAndAir.plateColors[self.random:integer(1, #LocksAndAir.plateColors)])
        self.plates[#self.plates + 1] = plate
        if #self.plates > LocksAndAir.maxPlates then
            table.remove(self.plates, 1):destroy()
        end
    end
end

-- Sets the speed and the lift of the air of the wind field.
function LocksAndAir:blow(speed, lift)
    self.speed, self.lift = speed, lift
    self.field.strength = speed
    self.field.lift = lift
end

function LocksAndAir:update(dt)
    LocksAndAir.super.update(self, dt)
    if input.pressed('reset') then
        self:build()
    end
    self.grab:update(self.pointer, self.camera)
    self.flash = math.max(0, (self.flash or 0) - dt * 4)
    self:status(string.format('The gate counted %d of %d shots. The wind blows at %d units a second with a lift of %.1f.', self.counted, self.fired, self.speed, self.lift))
end

function LocksAndAir:fixedUpdate(step)
    self:simulate(self.world, step)
end

function LocksAndAir:draw(area)
    Scenery.sky(self.camera, LocksAndAir.ground)
    Scenery.ground(LocksAndAir.ground)
    local unit = Look.pixel
    for _, x in ipairs({-LocksAndAir.wall - 20, -260, 260, LocksAndAir.wall + 20}) do
        Look.block(x, 0, 40, 900, 'stone', -2)
    end
    -- The lines the locks hold the lift and the sledge on.
    Look.dashed(LocksAndAir.liftX, -300, LocksAndAir.liftX, LocksAndAir.ground, '#99FFFFFF', -1)
    Look.dashed(-760, LocksAndAir.sledgeY, -280, LocksAndAir.sledgeY, '#99FFFFFF', -1)
    Look.drawAll({self.liftBody, self.sledge, self.turnstile})
    Look.drawAll(self.balls, {layer = 1})
    Look.drawAll(self.shots, {layer = 2})
    Look.drawAll(self.plates, {layer = 1})
    Look.draw(self.target)
    Look.block(LocksAndAir.cannon[1], LocksAndAir.cannon[2], 60, 30, 'iron', 3)
    Look.line(LocksAndAir.gateX, LocksAndAir.cannon[2] - 160, LocksAndAir.gateX, LocksAndAir.cannon[2] + 160, (6 + 6 * self.flash) * unit, self.flash > 0 and '#FFFFE6A0' or '#CC7BE6C9', 4)
    self:label('Motion locks', -520, -330)
    self:label('A sensor gate', 0, -330)
    self:label('Wind with lift', 520, -330)
    self.grab:draw()
end

return LocksAndAir
