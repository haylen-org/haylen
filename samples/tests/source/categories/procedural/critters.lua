-- Critters: bugs made from a seed and seen from above, each with its own kind of body, colors and pattern, the six legs of an insect on its thorax, antennae and sometimes wings. Every leg is a chain of three segments that grows from under the thorax and keeps its foot planted until the gait moves it, the legs step in two tripods, the front and back legs of one side with the middle leg of the other, and the antennae reach ahead and sway. The bugs wander, keep apart and run from the pointer, and a press scares the ones near it.
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local input = require('haylen.input')
local m = require('haylen.math')
local ui = require('haylen.ui')

local AlgorithmTest = require('categories.algorithms.algorithm-test')
local Leg = require('categories.procedural.leg')

local Critters = haylen.class('Critters', AlgorithmTest)

Critters.code = [[
local bug = {kind = pick({'beetle', 'ant', 'ladybug'}), hue = random:nextFloat(), wings = random:chance(0.3)}  -- Six legs on the thorax.
leg.restX, leg.restY = hipX + sideX * 26 * size + forwardX * reach, hipY + sideY * 26 * size + forwardY * reach
Leg.gait(bug.legs, 10 * size, velocityX * 0.08, velocityY * 0.08)  -- Two tripods of legs take turns.
if distance(bug, pointer) < 220 then bug.heading = angleAwayFrom(pointer) bug.speed = 260 end]]

Critters.kinds = {'beetle', 'ant', 'ladybug'}
Critters.legPairs = 3
Critters.outline = '#FF1E1B33'
-- The spots of a pattern, as shares of the half length and the half width of the wing cases, one of each pair on each side, so no two spots overlap.
Critters.spotPlaces = {{0.45, 0.45}, {-0.05, 0.62}, {-0.5, 0.42}, {0.1, 0.22}, {-0.65, 0.12}}
-- The ground spreads past what any screen shows, and the bugs keep to the part of it in view.
Critters.ground = {-1500, -900, 3000, 1800}

function Critters:enter()
    self.seed = 4
    self.count = 14
    self.bounds = {-760, -400, 1520, 800}
    self:build()
    self:frame{
        code = Critters.code,
        hint = 'Bugs run from the pointer, and a click or a tap scares every bug near it. R or the X button makes new bugs.',
        controls = {
            ui.formField{label = 'Bugs', ui.slider{id = 'count', min = 4, max = 40, step = 1, value = self.count, showValue = true, decimals = 0, onChange = function(event)
                self.count = math.tointeger(event.value)
                self:build()
            end}},
            ui.button{id = 'bugs', text = 'New bugs', variant = 'primary', onClick = function()
                self.seed = self.seed + 1
                self:build()
            end},
        },
    }
end

-- Scatters pebbles and tufts of grass over the ground, and makes the bugs.
function Critters:build()
    local random = m.random(self.seed)
    local ground = Critters.ground
    self.pebbles = {}
    for _ = 1, 360 do
        local shade = random:range(0.42, 0.7)
        self.pebbles[#self.pebbles + 1] = {random:range(ground[1], ground[1] + ground[3]), random:range(ground[2], ground[2] + ground[4]), random:range(3, 11), random:range(2, 7), m.color(shade, shade * 0.9, shade * 0.72, 1)}
    end
    self.tufts = {}
    for _ = 1, 180 do
        self.tufts[#self.tufts + 1] = {random:range(ground[1], ground[1] + ground[3]), random:range(ground[2], ground[2] + ground[4]), random:range(8, 18)}
    end
    self.bugs = {}
    for _ = 1, self.count do
        self.bugs[#self.bugs + 1] = Critters.make(random, self.bounds)
    end
    self.scares = 0
end

-- Makes one bug at a random place of the bounds.
function Critters.make(random, bounds)
    local bug = {x = random:range(bounds[1] + 60, bounds[1] + bounds[3] - 60), y = random:range(bounds[2] + 60, bounds[2] + bounds[4] - 60), heading = random:range(0, math.pi * 2)}
    bug.size = random:range(1.6, 2.3)
    bug.kind = Critters.kinds[random:integer(1, #Critters.kinds)]
    local hue = random:nextFloat()
    bug.shell = m.fromHsv(hue, random:range(0.45, 0.8), random:range(0.55, 0.9))
    bug.dark = m.fromHsv(hue, 0.6, 0.25)
    bug.spots = bug.kind == 'ladybug' and random:integer(2, 5) or (random:chance(0.3) and random:integer(1, 3) or 0)
    bug.wings = random:chance(0.3)
    bug.length = random:range(0.85, 1.5)
    bug.cruise = random:range(30, 60) * bug.size
    bug.speed = bug.cruise
    bug.turn = random:range(0, 100)
    bug.fear = 0
    local size = bug.size
    bug.legs = {}
    -- The three pairs grow close together from under the thorax, the front pair reaching ahead and the back pair behind.
    for pair = 1, Critters.legPairs do
        local along = 9 - (pair - 1) * 6
        for side = -1, 1, 2 do
            local leg = Leg({x = bug.x, y = bug.y, lengths = {9 * size, 10 * size, 8 * size}, group = (pair + (side > 0 and 1 or 0)) % 2 + 1, stepTime = 0.09, lift = 3, widths = {3.2 * size, 2.6 * size, 2 * size, 1.4 * size}, color = bug.dark, outlineWidth = 1.5, layer = 2})
            leg.along, leg.side = along * size, side
            leg.reach = (2 - pair) * 18 * size
            bug.legs[#bug.legs + 1] = leg
        end
    end
    bug.antennae = {}
    for side = -1, 1, 2 do
        local antenna = Leg({x = bug.x, y = bug.y, lengths = {9 * size, 8 * size, 6 * size}, widths = {1.3 * size, 1 * size, 0.8 * size, 0.6 * size}, color = bug.dark, outlineWidth = 1, layer = 2})
        antenna.side = side
        bug.antennae[#bug.antennae + 1] = antenna
    end
    Critters.placeLegs(bug, 0, true)
    return bug
end

-- Sets the hip, the rest point and the pole of every leg from the body, and the lead of a step from the velocity.
function Critters.placeLegs(bug, dt, settle)
    local size = bug.size
    local fx, fy = math.cos(bug.heading), math.sin(bug.heading)
    local sx, sy = -fy, fx
    for _, leg in ipairs(bug.legs) do
        local side = leg.side
        leg.hipX, leg.hipY = bug.x + fx * leg.along + sx * side * 4 * size, bug.y + fy * leg.along + sy * side * 4 * size
        leg.restX, leg.restY = leg.hipX + sx * side * 21 * size + fx * leg.reach, leg.hipY + sy * side * 21 * size + fy * leg.reach
        leg.poleX, leg.poleY = leg.hipX + sx * side * 18 * size + fx * leg.reach * 0.2, leg.hipY + sy * side * 18 * size + fy * leg.reach * 0.2
        leg.liftX, leg.liftY = sx * side, sy * side
        if settle then
            leg:place(leg.restX, leg.restY)
        end
    end
end

-- Steers a bug: it wanders, turns away from its neighbors and the edges, and runs from the pointer while it is afraid.
function Critters:steer(bug, dt, pointer, time)
    local bounds = self.bounds
    local desired = bug.heading + math.sin(time * 0.7 + bug.turn) * 1.6 * dt
    local awayX, awayY = 0, 0
    for _, other in ipairs(self.bugs) do
        local dx, dy = bug.x - other.x, bug.y - other.y
        local distance = dx * dx + dy * dy
        local reach = 46 * (bug.size + other.size)
        if other ~= bug and distance < reach * reach and distance > 0 then
            awayX, awayY = awayX + dx / distance, awayY + dy / distance
        end
    end
    local px, py = bug.x - pointer.worldX, bug.y - pointer.worldY
    if px * px + py * py < 220 * 220 then
        bug.fear = 1
    end
    if bug.fear > 0 then
        awayX, awayY = awayX + px / (px * px + py * py + 1) * 40, awayY + py / (px * px + py * py + 1) * 40
    end
    local margin = 80
    if bug.x < bounds[1] + margin then awayX = awayX + 1 end
    if bug.x > bounds[1] + bounds[3] - margin then awayX = awayX - 1 end
    if bug.y < bounds[2] + margin then awayY = awayY + 1 end
    if bug.y > bounds[2] + bounds[4] - margin then awayY = awayY - 1 end
    if awayX ~= 0 or awayY ~= 0 then
        desired = math.atan(awayY, awayX)
    end
    local turn = m.wrapAngle(desired - bug.heading)
    bug.heading = bug.heading + m.clamp(turn, -6 * dt, 6 * dt)
    bug.fear = math.max(0, bug.fear - dt * 0.6)
    bug.speed = m.lerp(bug.speed, bug.fear > 0 and 280 * bug.size or bug.cruise, m.dampFactor(4, dt))
end

function Critters:update(dt)
    Critters.super.update(self, dt)
    if not self.stage then
        return
    end
    if input.pressed('reset') then
        self.seed = self.seed + 1
        self:build()
    end
    -- The bugs keep to the part of the ground the play area shows.
    local view = self.view
    local zoom = math.min(self.stage.width / view[1], self.stage.height / view[2])
    local halfWidth, halfHeight = self.stage.width / zoom / 2 - 40, self.stage.height / zoom / 2 - 40
    self.bounds = {-halfWidth, -halfHeight, halfWidth * 2, halfHeight * 2}
    local pointer = self.pointer
    local time = haylen.elapsed()
    if pointer.pressed then
        local scared = 0
        for _, bug in ipairs(self.bugs) do
            if (bug.x - pointer.worldX) ^ 2 + (bug.y - pointer.worldY) ^ 2 < 360 * 360 then
                bug.fear, scared = 1.5, scared + 1
            end
        end
        self.scares = self.scares + 1
        self:log('Scared %d bugs.', scared)
    end

    local stepping, legs, running = 0, 0, 0
    for _, bug in ipairs(self.bugs) do
        self:steer(bug, dt, pointer, time)
        local vx, vy = math.cos(bug.heading) * bug.speed, math.sin(bug.heading) * bug.speed
        bug.x, bug.y = bug.x + vx * dt, bug.y + vy * dt
        Critters.placeLegs(bug, dt, false)
        Leg.gait(bug.legs, 9 * bug.size, vx * 0.08, vy * 0.08)
        for _, leg in ipairs(bug.legs) do
            leg:update(dt, leg.hipX, leg.hipY, leg.liftX, leg.liftY, leg.poleX, leg.poleY)
            stepping, legs = stepping + (leg:stepping() and 1 or 0), legs + 1
        end
        local fx, fy = math.cos(bug.heading), math.sin(bug.heading)
        local size = bug.size
        for _, antenna in ipairs(bug.antennae) do
            local side = antenna.side
            local hipX, hipY = bug.x + fx * 20 * size + -fy * side * 4 * size, bug.y + fy * 20 * size + fx * side * 4 * size
            local sway = math.sin(time * 6 + bug.turn + side) * 4 * size
            antenna:place(hipX + fx * 18 * size - fy * side * (10 * size + sway), hipY + fy * 18 * size + fx * side * (10 * size + sway))
            antenna:update(dt, hipX, hipY, 0, 0, hipX - fy * side * 30, hipY + fx * side * 30)
        end
        running = running + (bug.fear > 0 and 1 or 0)
    end
    self:status(string.format('%d bugs, %d of them running from the pointer, with %d of their %d legs in a step.', #self.bugs, running, stepping, legs))
end

-- A bug draws from the ground up: its soft shadow, its legs and antennae from under its body, the abdomen or the wing cases with their pattern, the thorax and the head, every part with the outline and the highlight of the art style.
function Critters.drawBug(bug, time)
    local size = bug.size
    local angle = bug.heading
    local fx, fy = math.cos(angle), math.sin(angle)
    local function ellipse(along, width, height, color, layer)
        local x, y = bug.x + fx * along * size, bug.y + fy * along * size
        graphics2d.drawShape({x - width * size / 2, y - height * size / 2, width * size, height * size}, {kind = 'ellipse', rotation = angle, color = color, borderWidth = 2.5, borderColor = Critters.outline, layer = layer})
    end
    local function shine(along, across, width, height, layer)
        local x, y = bug.x + (fx * along - fy * across) * size, bug.y + (fy * along + fx * across) * size
        graphics2d.drawShape({x - width * size / 2, y - height * size / 2, width * size, height * size}, {kind = 'ellipse', rotation = angle, color = '#80FFFFFF', layer = layer})
    end
    local abdomen = 30 * bug.length
    graphics2d.drawShape({bug.x - (abdomen * 0.6 + 14) * size, bug.y - 9 * size + 5, (abdomen * 1.2 + 28) * size, 18 * size}, {kind = 'ellipse', rotation = angle, color = '#40000000', softness = 6 * size, layer = 1})
    for _, leg in ipairs(bug.legs) do
        leg:draw()
    end
    for _, antenna in ipairs(bug.antennae) do
        antenna:draw()
    end
    if bug.kind == 'ant' then
        ellipse(-10 - abdomen * 0.35, abdomen * 0.9, abdomen * 0.65, bug.dark, 3)
        shine(-6 - abdomen * 0.45, -abdomen * 0.12, abdomen * 0.3, abdomen * 0.14, 3)
        ellipse(2, 13, 9, bug.shell, 3)
        ellipse(13, 13, 12, bug.dark, 3)
        shine(15, -2, 5, 3, 3)
    else
        local back = -abdomen * 0.25
        ellipse(back, abdomen + 12, 28, bug.shell, 3)
        local backX, backY = bug.x + fx * back * size, bug.y + fy * back * size
        local halfLength = (abdomen + 12) * 0.5
        graphics2d.drawLine(backX + fx * (halfLength - 6) * size, backY + fy * (halfLength - 6) * size, backX - fx * (halfLength - 2) * size, backY - fy * (halfLength - 2) * size, 2, Critters.outline, {layer = 3})
        for spot = 1, bug.spots do
            local place = Critters.spotPlaces[math.floor((spot - 1) / 2) % #Critters.spotPlaces + 1]
            local across = (spot % 2 == 0 and 1 or -1) * place[2] * 14
            local along = place[1] * halfLength
            graphics2d.drawCircle(backX + (fx * along - fy * across) * size, backY + (fy * along + fx * across) * size, 3.2 * size, bug.dark, {layer = 3})
        end
        shine(back + halfLength * 0.35, -7, halfLength * 0.6, 4, 3)
        ellipse(10, 14, 20, bug.dark, 3)
        ellipse(19, 11, 13, bug.dark, 3)
        shine(19, -2.5, 4, 2.5, 3)
    end
    if bug.wings then
        local open = bug.fear > 0 and (0.5 + math.sin(time * 60) * 0.3) or 0.15
        for side = -1, 1, 2 do
            local wingAngle = angle + math.pi + side * open
            local wx, wy = bug.x + math.cos(wingAngle) * 16 * size, bug.y + math.sin(wingAngle) * 16 * size
            graphics2d.drawShape({wx - 18 * size, wy - 7 * size, 36 * size, 14 * size}, {kind = 'ellipse', rotation = wingAngle, color = '#55DDEEFF', borderWidth = 1.5, borderColor = '#99FFFFFF', layer = 4})
        end
    end
end

-- The ground of packed earth fills the whole canvas, with pebbles and tufts of grass.
function Critters:draw(area)
    graphics2d.drawRect(graphics2d.canvasBounds(), '#FF8C7A58')
    for _, pebble in ipairs(self.pebbles) do
        graphics2d.drawEllipse(pebble[1], pebble[2], pebble[3], pebble[4], pebble[5])
    end
    for _, tuft in ipairs(self.tufts) do
        for blade = -1, 1 do
            graphics2d.drawLine(tuft[1] + blade * 3, tuft[2], tuft[1] + blade * 7, tuft[2] - tuft[3], 2.5, '#FF557E36')
        end
    end
    local time = haylen.elapsed()
    for _, bug in ipairs(self.bugs) do
        Critters.drawBug(bug, time)
    end
end

return Critters
