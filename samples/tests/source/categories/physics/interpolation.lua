-- One world drawn twice in slow motion. The world steps at the fixed rate of the app, so in slow motion several frames pass between two steps: on the left the bodies are drawn where the last step left them and move in jumps, and on the right the world interpolates and `body:renderTransform()` blends between the last two steps, so they glide. Ghosts of the last frames show the jumps on the left and the glide on the right.
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local input = require('haylen.input')
local physics2d = require('haylen.physics2d')
local ui = require('haylen.ui')

local Look = require('categories.physics.look')
local PhysicsTest = require('categories.physics.physics-test')
local Scenery = require('categories.physics.scenery')

local Interpolation = haylen.class('Interpolation', PhysicsTest)

Interpolation.code = [[
local world = physics2d.newWorld({interpolate = true})
local x, y, rotation = ball:renderTransform()  -- Between the last two steps.
haylen.setTimeScale(0.1)  -- Slow motion, with several frames for each fixed step.]]

Interpolation.offset = 400
Interpolation.radius = 34
Interpolation.bar = {width = 260, height = 24}
Interpolation.pivot = {0, -280}
Interpolation.hub = {0, 110}
Interpolation.length = 200
Interpolation.ground = 340
Interpolation.ghosts = 18

function Interpolation:enter()
    self:frame{
        code = Interpolation.code,
        hint = 'Slow the time down to see the steps. The world is the same on both sides, only the drawing differs. R or the X button starts over.',
        controls = {
            ui.label{text = 'Speed of the time', font = 'caption', color = 'textMuted'},
            ui.slider{id = 'scale', value = 0.1, min = 0.02, max = 1, step = 0.02, showValue = true, decimals = 2, onChange = function(event) haylen.setTimeScale(event.value) end},
            ui.button{id = 'reset', text = 'Start over', onClick = function() self:build() end},
        },
        pointer = false,
        focus = 'scale',
    }
    haylen.setTimeScale(0.1)
    self.trails = {}
    for index = 1, 2 do
        self.trails[index] = {count = 0, next = 1}
        for slot = 1, Interpolation.ghosts do
            self.trails[index][slot] = {0, 0, 0, 0}
        end
    end
    self.rope = {0, 0, 0, 0}
    self:build()
end

function Interpolation:build()
    self.world = physics2d.newWorld({interpolate = true})
    local pivot, hub = Interpolation.pivot, Interpolation.hub
    local anchor = self.world:createBody({type = 'static', x = pivot[1], y = pivot[2]})
    self.pendulum = self.world:createBody({x = pivot[1] + Interpolation.length, y = pivot[2]})
    self.pendulum:addCircle(Interpolation.radius, {density = 2})
    self.world:createJoint('distance', anchor, self.pendulum, {ax = pivot[1], ay = pivot[2], bx = pivot[1] + Interpolation.length, by = pivot[2]})

    local axle = self.world:createBody({type = 'static', x = hub[1], y = hub[2]})
    self.bar = self.world:createBody({x = hub[1], y = hub[2], gravityScale = 0})
    self.bar:addBox(Interpolation.bar.width, Interpolation.bar.height)
    self.world:createJoint('revolute', axle, self.bar, {ax = hub[1], ay = hub[2], enableMotor = true, motorSpeed = 3, maxMotorTorque = 1e9})

    local floor = self.world:createBody({type = 'static', x = 0, y = Interpolation.ground + 30})
    floor:addBox(8000, 60)
    self.ball = self.world:createBody({x = 270, y = 60})
    self.ball:addCircle(Interpolation.radius, {restitution = 0.9})
    for _, trail in ipairs(self.trails) do
        trail.count, trail.next = 0, 1
    end
end

function Interpolation:exit()
    Interpolation.super.exit(self)
    haylen.setTimeScale(1)
end

function Interpolation.stepped(body)
    return body.x, body.y, body.rotation
end

function Interpolation.blended(body)
    return body:renderTransform()
end

Interpolation.transforms = {Interpolation.stepped, Interpolation.blended}

-- Every frame adds the place of the pendulum and of the ball to the ghosts of each side, the oldest giving way.
function Interpolation:record()
    for index, trail in ipairs(self.trails) do
        local transform = Interpolation.transforms[index]
        local px, py = transform(self.pendulum)
        local bx, by = transform(self.ball)
        local slot = trail[trail.next]
        slot[1], slot[2], slot[3], slot[4] = px, py, bx, by
        trail.next = trail.next % Interpolation.ghosts + 1
        trail.count = math.min(trail.count + 1, Interpolation.ghosts)
    end
end

function Interpolation:update(dt)
    Interpolation.super.update(self, dt)
    if input.pressed('reset') then
        self:build()
    end
    self:record()
    local frames = 1 / math.max(haylen.timeScale(), 0.001) * haylen.fixedStep() * 60
    self:status(string.format('The time runs at %d%%, so a fixed step comes every %.1f frames, and the right side blends %d%% of the way to the next one.', math.floor(haylen.timeScale() * 100 + 0.5), frames, math.floor(haylen.interpolation() * 100 + 0.5)))
end

function Interpolation:fixedUpdate(step)
    self:simulate(self.world, step)
end

function Interpolation:drawSide(index)
    local offset = (index == 1 and -1 or 1) * Interpolation.offset
    local transform = Interpolation.transforms[index]
    local pivot, hub = Interpolation.pivot, Interpolation.hub
    local unit = Look.pixel
    local trail = self.trails[index]
    for age = 1, trail.count do
        local slot = trail[(trail.next - age - 1) % Interpolation.ghosts + 1]
        local alpha = 0.3 * (1 - age / (Interpolation.ghosts + 1))
        graphics2d.drawCircle(slot[1] + offset, slot[2], Interpolation.radius * 0.9, {0.12, 0.11, 0.2, alpha}, {layer = 0})
        graphics2d.drawCircle(slot[3] + offset, slot[4], Interpolation.radius * 0.9, {0.94, 0.28, 0.24, alpha}, {layer = 0})
    end

    local x, y = transform(self.pendulum)
    self.rope[1], self.rope[2], self.rope[3], self.rope[4] = pivot[1] + offset, pivot[2], x + offset, y
    Look.rope(self.rope, 8, 1)
    Look.block(pivot[1] + offset, pivot[2] - 14, 90, 18, 'darkWood', 2)
    Look.pin(pivot[1] + offset, pivot[2], 7 * unit, 3)
    Look.disc(x + offset, y, Interpolation.radius, 'iron', 2)

    local bx, by, rotation = transform(self.bar)
    Look.block(bx + offset, by, Interpolation.bar.width, Interpolation.bar.height, 'wood', 2, rotation)
    Look.pin(hub[1] + offset, hub[2], 9 * unit, 3)
    local cx, cy, spin = transform(self.ball)
    Look.disc(cx + offset, cy, Interpolation.radius, 'red', 2, spin)
end

function Interpolation:draw(area)
    Scenery.sky(self.camera, Interpolation.ground)
    Scenery.ground(Interpolation.ground)
    self:compare('Drawn where the last step left it', 'Interpolated between steps', 0, -352)
    self:tag('body.x, body.y, body.rotation', -Interpolation.offset, -318, {size = 16})
    self:tag('body:renderTransform()', Interpolation.offset, -318, {size = 16})
    self:drawSide(1)
    self:drawSide(2)
end

return Interpolation
