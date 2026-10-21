-- Centipede: a centipede seen from the side that crawls around a generated cave, up walls, across the ceiling and around rocks. Its head walks the surface at a fixed height and takes the shorter way around its rock to a point, turning around by lifting its head over its own body. Its body is one chain that follows the head with a bend limit and stays on the rock, and a pair of legs on every joint steps in a wave that runs from the head to the tail as it travels, with the long last pair trailing behind.
local animation2d = require('haylen.animation2d')
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local input = require('haylen.input')
local m = require('haylen.math')
local ui = require('haylen.ui')

local AlgorithmTest = require('categories.algorithms.algorithm-test')
local Cave = require('categories.procedural.cave')
local Crawler = require('categories.procedural.crawler')
local Leg = require('categories.procedural.leg')

local Centipede = haylen.class('Centipede', AlgorithmTest)

Centipede.code = [[
local body = animation2d.newChain({x = head.x, y = head.y, segments = 26, length = 22, maxBend = 0.45})
body:follow(head.x, head.y)  -- Every point trails the one before it.
if cave:distance(x, y) < height then body:setPoint(index, cave:project(x, y, height)) end  -- Never into the rock.
Leg.wave(legs, crawler.travelled, stride, 0.5, crawler.velocity, land)  -- leg.phase = -joint * 0.09: a wave from head to tail.]]

Centipede.height = 26
Centipede.stride = 23
Centipede.swing = 0.5
Centipede.segmentLength = 22
-- The share of a stride between the steps of two joints in a row, which sets how many joints one wave spans.
Centipede.wavePhase = 0.09
Centipede.turnRadius = 56
Centipede.view = {1240, 580}
Centipede.colors = {light = '#FFC8643F', dark = '#FFA84A2E', plate = '#FFE58A55', outline = '#FF1E1B33', leg = '#FFE7B070', far = '#FF8A5530', eye = '#FF141018', shine = '#FFFFFFFF', head = '#FF8A3322', fang = '#FF3A1A10', antenna = '#FFF2C88A', cercus = '#FF6E2A1C'}

function Centipede:enter()
    self.seed = 11
    self.speed = 150
    self.segments = 26
    self.showPoints = false
    self:build()
    self:frame{
        view = Centipede.view,
        code = Centipede.code,
        hint = 'Click or tap a point and the centipede crawls the shorter way around its rock to the nearest spot, turning around over its own body when it must. Without a point it wanders. R or the X button makes a new cave.',
        controls = {
            ui.formField{label = 'Speed', ui.slider{id = 'speed', min = 40, max = 360, step = 10, value = self.speed, showValue = true, decimals = 0, onChange = function(event) self.speed = event.value end}},
            ui.formField{label = 'Segments of the body', ui.slider{id = 'segments', min = 8, max = 44, step = 1, value = self.segments, showValue = true, decimals = 0, onChange = function(event)
                self.segments = math.tointeger(event.value)
                self:buildBody()
            end}},
            ui.checkbox{id = 'points', text = 'Show the points of the chain', checked = false, onChange = function(event) self.showPoints = event.checked end},
            ui.button{id = 'wander', text = 'Let it wander', onClick = function() self:setTarget(nil) end},
            ui.button{id = 'cave', text = 'New cave', variant = 'primary', onClick = function() self:newCave() end},
        },
    }
end

function Centipede:newCave()
    self.seed = self.seed + 1
    self:build()
end

function Centipede:build()
    -- A foothold lies ahead of the rest point along the joint the leg grows from, put on the rock.
    self.land = function(leg, lead)
        return self.cave:project(leg.restX + leg.leadX * lead, leg.restY + leg.leadY * lead, 0)
    end
    self.cave = Cave(self.seed, 1200, 540)
    self.crawler = Crawler(self.cave, -200, 400, Centipede.height, self.speed, self.seed)
    self.heading = self.crawler.dir
    self.target, self.turn = nil, nil
    self.turns = 0
    self:buildBody()
end

function Centipede:setTarget(x, y)
    self.target = x and {x, y} or nil
    self.crawler:setTarget(x, y)
end

-- Lays the body out on the rock behind the head with a pair of legs on every joint, longer toward the tail, and two antennae on the head.
function Centipede:buildBody()
    local crawler = self.crawler
    local tx, ty = crawler:tangent()
    self.body = animation2d.newChain({x = crawler.x, y = crawler.y, segments = self.segments, length = Centipede.segmentLength, angle = math.atan(-ty, -tx), maxBend = 0.45})
    for index = 2, self.body.count do
        self.body:setPoint(index, self.cave:walk(crawler.x, crawler.y, -(index - 1) * Centipede.segmentLength * crawler.dir, Centipede.height))
    end
    self.legs = {}
    local last = self.body.count - 1
    for index = 2, last do
        local scale = 1.3 * (1 + (index / last) * 0.3)
        for _, side in ipairs({'near', 'far'}) do
            local leg = Leg({x = crawler.x, y = crawler.y, lengths = {12 * scale, 14 * scale}, phase = -index * Centipede.wavePhase + (side == 'near' and 0 or 0.5), lift = 10, widths = {5, 4, 2.5}})
            leg.joint, leg.side = index, side
            self.legs[#self.legs + 1] = leg
        end
    end
    -- Long thin antennae feel the way ahead, and two short, thick cerci at the tail end, so the head and the tail never look alike.
    self.antennae = {}
    for _ = 1, 2 do
        self.antennae[#self.antennae + 1] = Leg({x = crawler.x, y = crawler.y, lengths = {24, 22, 18}, widths = {3, 2.5, 2, 1.2}, color = Centipede.colors.antenna, outlineWidth = 1.5, layer = 6})
    end
    self.tails = {}
    for _ = 1, 2 do
        self.tails[#self.tails + 1] = Leg({x = crawler.x, y = crawler.y, lengths = {12, 9}, widths = {6, 4.5, 2}, color = Centipede.colors.cercus, layer = 4})
    end
    self:placeBody(0)
    for _, leg in ipairs(self.legs) do
        leg:place(leg.restX, leg.restY)
    end
end

-- Returns where the head is: on the crawler, or on the arc that lifts it over its body while it turns around.
function Centipede:headPoint()
    local crawler, turn = self.crawler, self.turn
    if not turn then
        return crawler.x, crawler.y
    end
    local angle = math.min(1, turn.time / turn.duration) * math.pi
    local radius = Centipede.turnRadius
    local centerX, centerY = turn.x - turn.tx * radius, turn.y - turn.ty * radius
    return centerX + turn.tx * radius * math.cos(angle) + turn.nx * radius * 0.55 * math.sin(angle), centerY + turn.ty * radius * math.cos(angle) + turn.ny * radius * 0.55 * math.sin(angle)
end

-- Pulls the body after the head and keeps it out of the rock and close to it, then sets the hip, the rest point, the lead and the pole of every leg from the joint it grows from. A joint lifted off the rock lets its legs dangle.
function Centipede:placeBody(dt)
    local cave, body, height = self.cave, self.body, Centipede.height
    body:follow(self:headPoint())
    local settle = self.turn and 0 or m.dampFactor(8, dt)
    for index = 2, body.count do
        local x, y = body:point(index)
        local distance = cave:distance(x, y)
        if distance < height then
            body:setPoint(index, cave:project(x, y, height))
        elseif settle > 0 then
            local groundX, groundY = cave:project(x, y, height)
            body:setPoint(index, x + (groundX - x) * settle, y + (groundY - y) * settle)
        end
    end
    for _, leg in ipairs(self.legs) do
        local x, y = body:point(leg.joint)
        local aheadX, aheadY = body:point(leg.joint - 1)
        local tx, ty = aheadX - x, aheadY - y
        local length = math.max(1e-4, math.sqrt(tx * tx + ty * ty))
        tx, ty = tx / length, ty / length
        local nx, ny = cave:normal(x, y)
        local reach = leg.side == 'near' and 6 or -4
        leg.hipX, leg.hipY = x - nx * 5, y - ny * 5
        leg.restX, leg.restY = cave:project(x - nx * height + tx * reach, y - ny * height + ty * reach, 0)
        leg.leadX, leg.leadY = tx, ty
        -- The knee rises a little over the hip and leans forward, so each leg arches beside the body.
        leg.poleX, leg.poleY = leg.hipX + tx * 10 + nx * 16, leg.hipY + ty * 10 + ny * 16
        leg.nx, leg.ny = nx, ny
        leg.airborne = cave:distance(x, y) > height * 1.8
        if leg.airborne then
            leg:place(leg.hipX - nx * 14 - tx * 6, leg.hipY - ny * 14 - ty * 6)
        end
    end
end

-- Starts lifting the head over the body once the crawler has stopped to turn around, and lands it on the rock behind when the arc ends.
function Centipede:updateTurn(dt)
    local crawler = self.crawler
    if not self.turn and crawler.dir ~= self.heading then
        local nx, ny = crawler.nx, crawler.ny
        self.turn = {x = crawler.x, y = crawler.y, nx = nx, ny = ny, tx = -ny * self.heading, ty = nx * self.heading, time = 0, duration = m.clamp(math.pi * Centipede.turnRadius / math.max(self.speed, 1), 0.6, 1.4)}
        self.heading = crawler.dir
        self.turns = self.turns + 1
    end
    local turn = self.turn
    if not turn then
        return false
    end
    turn.time = turn.time + dt
    if turn.time >= turn.duration then
        crawler:land(self:headPoint())
        self.turn = nil
        return false
    end
    return true
end

function Centipede:update(dt)
    Centipede.super.update(self, dt)
    if not self.stage then
        return
    end
    if input.pressed('reset') then
        self:newCave()
    end
    if self.pointer.pressed then
        self:setTarget(self.pointer.worldX, self.pointer.worldY)
    end

    local crawler = self.crawler
    crawler.speed = self.speed
    if not self:updateTurn(dt) then
        crawler:update(dt)
        self:updateTurn(0)
    end
    self:placeBody(dt)
    Leg.wave(self.legs, crawler.travelled, Centipede.stride, Centipede.swing, crawler.velocity, self.land)
    local stepping = 0
    for _, leg in ipairs(self.legs) do
        leg:update(dt, leg.hipX, leg.hipY, leg.nx, leg.ny, leg.poleX, leg.poleY)
        stepping = stepping + (leg:stepping() and 1 or 0)
    end
    self:placeFeelers(dt)

    local state = 'wandering'
    if self.turn then
        state = 'turning around'
    elseif self.target then
        state = crawler.arrived and (crawler.unreachable and 'as near as it gets, since the point is on another rock' or 'at the point') or 'crawling to the point'
    end
    local turns = self.turns == 1 and 'once' or self.turns .. ' times'
    self:status(string.format('In cave %d, the centipede of %d segments is %s. It has %d of its %d legs in a step and turned around %s.', self.seed, self.segments, state, stepping, #self.legs, turns))
end

-- Sways the antennae ahead of the head, each to its own side, and trails the last pair of legs behind the tail.
function Centipede:placeFeelers(dt)
    local body = self.body
    local time = haylen.elapsed()
    local headX, headY = body:point(1)
    local neckX, neckY = body:point(2)
    local tx, ty = headX - neckX, headY - neckY
    local length = math.max(1e-4, math.sqrt(tx * tx + ty * ty))
    tx, ty = tx / length, ty / length
    local nx, ny = ty * self.heading, -tx * self.heading
    for index, antenna in ipairs(self.antennae) do
        local spread = (index == 1 and 1 or -1) * 12 + math.sin(time * 5 + index * 2) * 9
        local hipX, hipY = headX + tx * 14 + nx * 6, headY + ty * 14 + ny * 6
        antenna:place(hipX + tx * 54 + nx * (18 + spread), hipY + ty * 54 + ny * (18 + spread))
        antenna:update(dt, hipX, hipY, nx, ny, hipX + tx * 18 + nx * 56, hipY + ty * 18 + ny * 56)
    end
    local tailX, tailY = body:point(body.count)
    local beforeX, beforeY = body:point(body.count - 1)
    tx, ty = tailX - beforeX, tailY - beforeY
    length = math.max(1e-4, math.sqrt(tx * tx + ty * ty))
    tx, ty = tx / length, ty / length
    for index, tail in ipairs(self.tails) do
        local spread = (index == 1 and 1 or -1) * 5 + math.sin(time * 3 + index) * 2
        tail:place(tailX + tx * 20 + nx * (spread - 2), tailY + ty * 20 + ny * (spread - 2))
        tail:update(dt, tailX, tailY, nx, ny, tailX + tx * 10 + nx * 8, tailY + ty * 10 + ny * 8)
    end
end

function Centipede:draw(area)
    self.cave:draw()
    local colors = Centipede.colors
    if self.target then
        local goalX, goalY = self.cave:project(self.target[1], self.target[2], 0)
        graphics2d.drawRing(goalX, goalY, 12, 3, '#FFF2B23A', {layer = 3})
    end
    -- The legs grow from under the body, the far ones darker behind the near ones.
    for _, leg in ipairs(self.legs) do
        local near = leg.side == 'near'
        leg:draw(near and colors.leg or colors.far, near and 4 or 2)
    end
    for index, antenna in ipairs(self.antennae) do
        antenna:draw(index == 1 and colors.antenna or colors.far, index == 1 and 7 or 2)
    end
    for index, tail in ipairs(self.tails) do
        tail:draw(index == 1 and colors.cercus or colors.far, index == 1 and 4 or 2)
    end

    -- The body draws from the tail to the head, so each plate overlaps the one behind it, with the light of its back plate on the side away from the rock, and the head carries its eye and fangs.
    local body = self.body
    for index = body.count, 1, -1 do
        local x, y = body:point(index)
        local angle = body:angle(math.min(index, body.count - 1)) + math.pi
        local taper = index == 1 and 1.2 or 1 - (index / body.count) * 0.3
        local width, height = 32 * taper, 24 * taper
        local upX, upY = self.cave:normal(x, y)
        local color = index == 1 and colors.head or (index % 2 == 0 and colors.dark or colors.light)
        graphics2d.drawShape({x - width / 2, y - height / 2, width, height}, {kind = 'ellipse', rotation = angle, color = color, borderWidth = 3, borderColor = colors.outline, layer = 5})
        local plateX, plateY = x + upX * height * 0.18, y + upY * height * 0.18
        graphics2d.drawShape({plateX - width * 0.32, plateY - height * 0.2, width * 0.64, height * 0.4}, {kind = 'ellipse', rotation = angle, color = colors.plate, layer = 5})
        if self.showPoints then
            graphics2d.drawRing(x, y, 4, 1.5, '#FF8FB0FF', {layer = 30})
        end
    end
    local headX, headY = body:point(1)
    local neckX, neckY = body:point(2)
    local tx, ty = headX - neckX, headY - neckY
    local length = math.max(1e-4, math.sqrt(tx * tx + ty * ty))
    tx, ty = tx / length, ty / length
    local nx, ny = ty * self.heading, -tx * self.heading
    graphics2d.drawStroke({headX + tx * 12 - nx * 5, headY + ty * 12 - ny * 5, headX + tx * 21 - nx * 12, headY + ty * 21 - ny * 12}, {width = 4, color = colors.fang, cap = 'round', layer = 6})
    graphics2d.drawCircle(headX + tx * 7 + nx * 5, headY + ty * 7 + ny * 5, 5, colors.outline, {layer = 6})
    graphics2d.drawCircle(headX + tx * 7 + nx * 5, headY + ty * 7 + ny * 5, 3.8, colors.eye, {layer = 6})
    graphics2d.drawCircle(headX + tx * 8 + nx * 6.5, headY + ty * 8 + ny * 6.5, 1.5, colors.shine, {layer = 6})
end

return Centipede
