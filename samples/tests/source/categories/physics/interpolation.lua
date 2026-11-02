-- One world drawn twice in slow motion. The world steps at the fixed rate of the app, so in slow motion several frames pass between two steps: on the left the bodies are drawn where the last step left them and move in jumps, and on the right the world interpolates and `body:renderTransform()` blends between the last two steps, so they glide. Over the last few steps, the left side leaves a ghost at each place a step put a body, a row of separate jumps, and the right side leaves a smooth tail through every frame.
local assets = require('haylen.assets')
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
Interpolation.pivot = {0, -262}
Interpolation.hub = {0, 130}
Interpolation.length = 200
Interpolation.ground = 340
-- The pendulum hangs from the arm of a wooden post that stands this far to the left of its pivot.
Interpolation.post = 300
-- The trails cover this many of the last steps, and the tail of the right side holds up to this many frames.
Interpolation.steps = 8
Interpolation.frames = 240
-- Each trail follows the bob of the pendulum, the ball and the end of the bar, with the color and the size of its ghosts.
Interpolation.trails = {
    {color = {0.25, 0.27, 0.38}, radius = 30},
    {color = {0.94, 0.28, 0.24}, radius = 30},
    {color = {0.82, 0.52, 0.24}, radius = 9},
}

function Interpolation:enter()
    self:frame{
        code = Interpolation.code,
        hint = 'Slow the time down to see the steps: the ghosts on the left show a jump at every step, and the tail on the right shows the glide in between. The world is the same on both sides, only the drawing differs. R or the X button starts over.',
        controls = {
            ui.label{text = 'Speed of the time', font = 'caption', color = 'textMuted'},
            ui.slider{id = 'scale', value = 0.1, min = 0.02, max = 1, step = 0.02, showValue = true, decimals = 2, onChange = function(event) haylen.setTimeScale(event.value) end},
            ui.button{id = 'reset', text = 'Start over', onClick = function() self:build() end},
        },
        pointer = false,
        focus = 'scale',
    }
    haylen.setTimeScale(0.1)
    self.codeFont = assets.font('fonts/space-mono-regular.ttf', {bakeSize = 64, spread = 16})
    self.codeText = {size = 1, color = '#FFE8EAF2', anchor = {0.5, 0.5}, layer = 41}
    self.codePlate = {kind = 'rectangle', radius = 0, color = '#E6232938', borderWidth = 0, borderColor = '#FF15172A', layer = 40}
    self.codeRect = {0, 0, 0, 0}
    self.rope = {0, 0, 0, 0}
    self.ghost = {0, 0, 0, 0}
    self.ghostOrder = {layer = 0}

    -- The places of the last steps and of the last frames, in rings of six numbers each, and the tables each tail fills in place.
    self.stepRing = {next = 1, count = 0}
    for slot = 1, Interpolation.steps + 1 do
        self.stepRing[slot] = {0, 0, 0, 0, 0, 0}
    end
    self.frameRing = {next = 1, count = 0}
    for slot = 1, Interpolation.frames do
        self.frameRing[slot] = {0, 0, 0, 0, 0, 0, time = 0}
    end
    self.tails = {}
    for index = 1, #Interpolation.trails do
        local tail = {points = {}, widths = {}, colors = {}, pool = {}}
        tail.stroke = {widths = tail.widths, colors = tail.colors, cap = 'round', join = 'round', layer = 0}
        self.tails[index] = tail
    end
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
    self.stepCount = 0
    self.stepRing.next, self.stepRing.count = 1, 0
    self.frameRing.next, self.frameRing.count = 1, 0
end

function Interpolation:exit()
    Interpolation.super.exit(self)
    haylen.setTimeScale(1)
end

-- Writes the places of the bob, the ball and the end of the bar into the next slot of a ring, the oldest giving way.
function Interpolation:record(ring, capacity, px, py, bx, by, rotation)
    local slot = ring[ring.next]
    local reach = Interpolation.bar.width / 2
    local hub = Interpolation.hub
    slot[1], slot[2], slot[3], slot[4] = px, py, bx, by
    slot[5], slot[6] = hub[1] + math.cos(rotation) * reach, hub[2] + math.sin(rotation) * reach
    ring.next = ring.next % capacity + 1
    ring.count = math.min(ring.count + 1, capacity)
    return slot
end

-- Returns the slot of a ring that was written `age` writes before the newest one.
function Interpolation.slot(ring, capacity, age)
    return ring[(ring.next - age - 2) % capacity + 1]
end

function Interpolation:update(dt)
    Interpolation.super.update(self, dt)
    if input.pressed('reset') then
        self:build()
    end

    -- A frame of the right side shows the world between the last two steps, at that time of the world.
    local px, py = self.pendulum:renderTransform()
    local bx, by = self.ball:renderTransform()
    local _, _, rotation = self.bar:renderTransform()
    local slot = self:record(self.frameRing, Interpolation.frames, px, py, bx, by, rotation)
    slot.time = (self.stepCount - 1 + haylen.interpolation()) * haylen.fixedStep()

    local seconds = haylen.fixedStep() / math.max(haylen.timeScale(), 0.001)
    self:status(string.format('The time runs at %d%%, so a fixed step comes every %.2f seconds, and the right side blends %d%% of the way to the next one.', math.floor(haylen.timeScale() * 100 + 0.5), seconds, math.floor(haylen.interpolation() * 100 + 0.5)))
end

function Interpolation:fixedUpdate(step)
    self:simulate(self.world, step)
    self.stepCount = self.stepCount + 1
    self:record(self.stepRing, Interpolation.steps + 1, self.pendulum.x, self.pendulum.y, self.ball.x, self.ball.y, self.bar.rotation)
end

-- The ghosts of the left side: one faint copy of each body at each of the last steps before the current one.
function Interpolation:drawGhosts(offset)
    local capacity = Interpolation.steps + 1
    local order = self.ghostOrder
    for age = 1, self.stepRing.count - 1 do
        local slot = Interpolation.slot(self.stepRing, capacity, age)
        local fade = 0.5 * (1 - age / capacity)
        for index, trail in ipairs(Interpolation.trails) do
            local color = self.ghost
            color[1], color[2], color[3], color[4] = trail.color[1], trail.color[2], trail.color[3], fade
            graphics2d.drawCircle(slot[index * 2 - 1] + offset, slot[index * 2], trail.radius, color, order)
        end
    end
end

-- The tails of the right side: a line through the place of each body in every frame over the same last steps, widest at the newest frame and fading away toward the oldest.
function Interpolation:drawTails(offset)
    local ring = self.frameRing
    local newest = Interpolation.slot(ring, Interpolation.frames, 0)
    local since = newest.time - Interpolation.steps * haylen.fixedStep()
    local count = 0
    while count < ring.count and Interpolation.slot(ring, Interpolation.frames, count).time >= since do
        count = count + 1
    end
    for index, trail in ipairs(Interpolation.trails) do
        local tail = self.tails[index]
        local points, widths, colors, pool = tail.points, tail.widths, tail.colors, tail.pool
        for point = 1, count do
            local slot = Interpolation.slot(ring, Interpolation.frames, count - point)
            local share = point / count
            points[point * 2 - 1], points[point * 2] = slot[index * 2 - 1] + offset, slot[index * 2]
            widths[point] = trail.radius * 1.6 * share
            pool[point] = pool[point] or {0, 0, 0, 0}
            local color = pool[point]
            color[1], color[2], color[3], color[4] = trail.color[1], trail.color[2], trail.color[3], 0.5 * share
            colors[point] = color
        end
        for extra = count + 1, #widths do
            points[extra * 2 - 1], points[extra * 2], widths[extra], colors[extra] = nil, nil, nil, nil
        end
        if count >= 2 then
            graphics2d.drawStroke(points, tail.stroke)
        end
    end
end

-- The pendulum hangs from the arm of a wooden post with a brace, and the bar turns on a post of its own.
function Interpolation:drawFrame(offset)
    local pivot, hub, ground = Interpolation.pivot, Interpolation.hub, Interpolation.ground
    local x = pivot[1] + offset - Interpolation.post
    local top = pivot[2] - 24
    Look.block(x, (top + ground) / 2, 22, ground - top, 'darkWood', 1)
    Look.block((x + pivot[1] + offset + 30) / 2, pivot[2] - 14, Interpolation.post + 52, 18, 'darkWood', 2)
    local fromX, fromY, toX, toY = x, pivot[2] + 80, x + 90, pivot[2] - 6
    Look.block((fromX + toX) / 2, (fromY + toY) / 2, math.sqrt((toX - fromX) ^ 2 + (toY - fromY) ^ 2), 12, 'darkWood', 1, math.atan(toY - fromY, toX - fromX))
    Look.block(hub[1] + offset, (hub[2] + ground) / 2, 18, ground - hub[2], 'darkWood', 1)
end

function Interpolation:drawSide(index)
    local offset = (index == 1 and -1 or 1) * Interpolation.offset
    local transform = index == 1 and Interpolation.stepped or Interpolation.blended
    local pivot, hub = Interpolation.pivot, Interpolation.hub
    local unit = Look.pixel
    self:drawFrame(offset)
    if index == 1 then
        self:drawGhosts(offset)
    else
        self:drawTails(offset)
    end

    local x, y = transform(self.pendulum)
    self.rope[1], self.rope[2], self.rope[3], self.rope[4] = pivot[1] + offset, pivot[2], x + offset, y
    Look.rope(self.rope, 8, 1)
    Look.pin(pivot[1] + offset, pivot[2], 7 * unit, 3)
    Look.disc(x + offset, y, Interpolation.radius, 'iron', 2)

    local bx, by, rotation = transform(self.bar)
    Look.block(bx + offset, by, Interpolation.bar.width, Interpolation.bar.height, 'wood', 2, rotation)
    Look.pin(hub[1] + offset, hub[2], 9 * unit, 3)
    local cx, cy, spin = transform(self.ball)
    Look.disc(cx + offset, cy, Interpolation.radius, 'red', 2, spin)
end

function Interpolation.stepped(body)
    return body.x, body.y, body.rotation
end

function Interpolation.blended(body)
    return body:renderTransform()
end

-- Draws a call of the API in the monospace font of the code card, on a dark pill at `x`, `y` of the world, which keeps its size on the screen at any zoom.
function Interpolation:codeTag(text, x, y)
    local unit = Look.pixel
    local style = self.codeText
    style.size = 16 * unit
    local width, height = graphics2d.measureText(self.codeFont, text, style)
    local plateWidth, plateHeight = width + 22 * unit, height + 10 * unit
    local rect = self.codeRect
    rect[1], rect[2], rect[3], rect[4] = x - plateWidth / 2, y - plateHeight / 2, plateWidth, plateHeight
    self.codePlate.radius, self.codePlate.borderWidth = plateHeight / 2, 2 * unit
    graphics2d.drawShape(rect, self.codePlate)
    graphics2d.drawText(self.codeFont, text, x, y, style)
end

function Interpolation:draw(area)
    Scenery.sky(self.camera, Interpolation.ground)
    Scenery.ground(Interpolation.ground)
    self:compare('Drawn where the last step left it', 'Interpolated between steps', 0, -352)
    self:codeTag('body.x, body.y, body.rotation', -Interpolation.offset, -316)
    self:codeTag('body:renderTransform()', Interpolation.offset, -316)
    self:drawSide(1)
    self:drawSide(2)
end

return Interpolation
