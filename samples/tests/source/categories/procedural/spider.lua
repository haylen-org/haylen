-- Spider: a spider with eight legs that climbs a generated cave over its floor, walls, ceiling and floating rocks. Its body walks the surface at a fixed height, every leg is a chain of two segments that reaches a foothold with its knee toward a pole, and the feet step in two alternating groups when the body has carried them too far. A point on another rock makes it leap across on a silk thread.
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local input = require('haylen.input')
local m = require('haylen.math')
local ui = require('haylen.ui')

local AlgorithmTest = require('categories.algorithms.algorithm-test')
local Cave = require('categories.procedural.cave')
local Crawler = require('categories.procedural.crawler')
local Leg = require('categories.procedural.leg')

local Spider = haylen.class('Spider', AlgorithmTest)

Spider.code = [[
local leg = animation2d.newChain({x = hipX, y = hipY, lengths = {62, 72}})
leg:reach(footX, footY, {pole = {hipX + normalX * 96, hipY + normalY * 96}})  -- The knee bends away from the rock.
if distance(foot, rest) > stride and not otherGroupStepping then stepTo(rest + velocity * 0.12) end
x, y = cave:project(x + tangentX * speed * dt, y + tangentY * speed * dt, height)  -- Walks walls and ceilings alike.]]

Spider.height = 36
Spider.stride = 40
-- The legs grow from the front part of the body, this far ahead of its center.
Spider.hipCenter = 16
Spider.view = {1300, 700}
-- Each leg: where its hip sits along the body, where its foot rests along the body, the side it is on and its group of the gait.
Spider.legs = {
    {hip = 12, foot = 94, side = 'near', group = 1}, {hip = 5, foot = 41, side = 'near', group = 2}, {hip = -2, foot = -31, side = 'near', group = 1}, {hip = -10, foot = -84, side = 'near', group = 2},
    {hip = 14, foot = 110, side = 'far', group = 2}, {hip = 7, foot = 58, side = 'far', group = 1}, {hip = 0, foot = -14, side = 'far', group = 2}, {hip = -7, foot = -67, side = 'far', group = 1},
}
Spider.colors = {near = '#FF9A6A48', far = '#FF5E3E2C', body = '#FF8A5A3C', back = '#FF6E4630', outline = '#FF24160F', mark = '#FFF2B23A', eye = '#FFFFF1D6', silk = '#FFE8EAF2'}

function Spider:enter()
    self.seed = 3
    self.speed = 230
    self.showGuides = false
    self:build()
    self:frame{
        view = Spider.view,
        code = Spider.code,
        hint = 'Click or tap a point and the spider climbs to the nearest spot of its rock, or leaps across on a thread when the point is on another rock. Without a point it wanders. R or the X button makes a new cave.',
        controls = {
            ui.formField{label = 'Speed', ui.slider{id = 'speed', min = 60, max = 500, step = 10, value = self.speed, showValue = true, decimals = 0, onChange = function(event) self.speed = event.value end}},
            ui.checkbox{id = 'guides', text = 'Show the rest points and the poles', checked = false, onChange = function(event) self.showGuides = event.checked end},
            ui.button{id = 'wander', text = 'Let it wander', onClick = function() self.target = nil end},
            ui.button{id = 'cave', text = 'New cave', variant = 'primary', onClick = function() self:newCave() end},
        },
    }
end

function Spider:newCave()
    self.seed = self.seed + 1
    self:build()
end

function Spider:build()
    self.cave = Cave(self.seed, 1240, 640)
    self.crawler = Crawler(self.cave, 0, 400, Spider.height, self.speed, self.seed)
    self.legs = {}
    for _, layout in ipairs(Spider.legs) do
        local leg = Leg({x = self.crawler.x, y = self.crawler.y, lengths = {62, 72}, group = layout.group, stepTime = 0.11, lift = 18, widths = {10, 8, 4}})
        leg.layout = layout
        self.legs[#self.legs + 1] = leg
    end
    self:placeLegs(true)
    self.target, self.leap, self.thread = nil, nil, nil
    self.leaps, self.steps = 0, 0
    self.leapTime = 6
end

-- Sets the hip, the rest point and the pole of every leg from the body, and puts the feet down at their rest points when `settle` is set.
function Spider:placeLegs(settle)
    local crawler = self.crawler
    local nx, ny = crawler.nx, crawler.ny
    local tx, ty = crawler:tangent()
    local groundX, groundY = crawler.x - nx * Spider.height, crawler.y - ny * Spider.height
    for _, leg in ipairs(self.legs) do
        local layout = leg.layout
        local along = Spider.hipCenter + layout.hip * 0.5
        leg.hipX, leg.hipY = crawler.x + tx * along + nx * 4, crawler.y + ty * along + ny * 4
        leg.restX, leg.restY = self.cave:project(groundX + tx * layout.foot, groundY + ty * layout.foot, 0)
        local outward = layout.foot >= 0 and 1 or -1
        leg.poleX, leg.poleY = leg.hipX + nx * 96 + tx * outward * 36, leg.hipY + ny * 96 + ty * outward * 36
        if settle then
            leg:place(leg.restX, leg.restY)
        end
    end
end

-- Starts a leap to the surface under a point when the straight line there stays clear of the rocks.
function Spider:tryLeap(x, y)
    local cave, crawler = self.cave, self.crawler
    local goalX, goalY = cave:project(x, y, Spider.height)
    local distance = math.sqrt((goalX - crawler.x) ^ 2 + (goalY - crawler.y) ^ 2)
    if distance < 120 or not cave:clear(crawler.x, crawler.y, goalX, goalY, Spider.height * 0.6) then
        return false
    end
    self.leap = {fromX = crawler.x, fromY = crawler.y, toX = goalX, toY = goalY, time = 0, duration = m.clamp(distance / 700, 0.3, 1.2), distance = distance}
    self.thread = {x = crawler.x - crawler.nx * Spider.height, y = crawler.y - crawler.ny * Spider.height, life = 1}
    self.leaps = self.leaps + 1
    self:log('Leap %d of %.0f units on a thread.', self.leaps, distance)
    return true
end

function Spider:updateLeap(dt)
    local leap, crawler = self.leap, self.crawler
    leap.time = leap.time + dt
    local t = math.min(1, leap.time / leap.duration)
    crawler.x = leap.fromX + (leap.toX - leap.fromX) * t
    crawler.y = leap.fromY + (leap.toY - leap.fromY) * t - math.sin(t * math.pi) * leap.distance * 0.12
    if t >= 1 then
        crawler:land(leap.toX, leap.toY)
        self.leap = nil
        return
    end
    -- The legs fold under the body while it flies.
    self:placeLegs(false)
    for _, leg in ipairs(self.legs) do
        local tx, ty = crawler:tangent()
        leg:place(leg.hipX - crawler.nx * 16 + tx * leg.layout.foot * 0.3, leg.hipY - crawler.ny * 16 + ty * leg.layout.foot * 0.3)
    end
end

function Spider:update(dt)
    Spider.super.update(self, dt)
    if not self.stage then
        return
    end
    if input.pressed('reset') then
        self:newCave()
    end
    local pointer = self.pointer
    if pointer.pressed then
        self.target = {pointer.worldX, pointer.worldY}
    end

    local crawler = self.crawler
    crawler.speed = self.speed
    if self.leap then
        self:updateLeap(dt)
    else
        local target = self.target
        crawler:update(dt, target and target[1], target and target[2])
        if target and crawler.stuck then
            self:tryLeap(target[1], target[2])
        elseif not target then
            self.leapTime = self.leapTime - dt
            if self.leapTime <= 0 then
                self.leapTime = crawler.random:range(5, 9)
                self:tryLeap(crawler.random:range(-560, 560), crawler.random:range(-280, 280))
            end
        end
    end
    if self.thread then
        self.thread.life = self.thread.life - (self.leap and 0 or dt / 1.5)
        self.thread = self.thread.life > 0 and self.thread or nil
    end

    self:placeLegs(false)
    if not self.leap then
        local tx, ty = crawler:tangent()
        local lead = crawler.moved / math.max(dt, 1e-4) * 0.12
        Leg.gait(self.legs, Spider.stride, tx * lead, ty * lead, function(x, y) return self.cave:project(x, y, 0) end)
    end
    local stepping = 0
    for _, leg in ipairs(self.legs) do
        leg:update(dt, leg.hipX, leg.hipY, crawler.nx, crawler.ny, leg.poleX, leg.poleY)
        stepping = stepping + (leg:stepping() and 1 or 0)
    end
    local state = self.leap and 'leaping' or (self.target and (crawler.arrived and 'arrived' or 'climbing to the point') or 'wandering')
    self:status(string.format('The spider is %s, legs stepping %d of 8, leaps %d, cave %d', state, stepping, self.leaps, self.seed))
end

function Spider:drawBody()
    local crawler = self.crawler
    local nx, ny = crawler.nx, crawler.ny
    local tx, ty = crawler:tangent()
    local angle = math.atan(ty, tx)
    local x, y = crawler.x, crawler.y
    local colors = Spider.colors
    local function ellipse(cx, cy, width, height, color, layer)
        graphics2d.drawShape({cx - width / 2, cy - height / 2, width, height}, {kind = 'ellipse', rotation = angle, color = color, borderWidth = 3, borderColor = colors.outline, layer = layer})
    end
    ellipse(x - tx * 38 + nx * 12, y - ty * 38 + ny * 12, 88, 66, colors.back, 5)
    ellipse(x - tx * 40 + nx * 22, y - ty * 40 + ny * 22, 34, 14, colors.mark, 5)
    ellipse(x - tx * 52 + nx * 14, y - ty * 52 + ny * 14, 16, 8, colors.mark, 5)
    ellipse(x + tx * 18 + nx * 5, y + ty * 18 + ny * 5, 54, 40, colors.body, 5)
    for index = -1, 1, 2 do
        graphics2d.drawCircle(x + tx * (36 + index * 4) + nx * (14 + index * 3), y + ty * (36 + index * 4) + ny * (14 + index * 3), 4.5, colors.eye, {layer = 5})
    end
    graphics2d.drawLine(x + tx * 40, y + ty * 40, x + tx * 50 - nx * 10, y + ty * 50 - ny * 10, 4, colors.outline, {layer = 5})
end

function Spider:draw(area)
    self.cave:draw()
    if self.target then
        local goalX, goalY = self.cave:project(self.target[1], self.target[2], 0)
        graphics2d.drawRing(goalX, goalY, 14, 3, Spider.colors.mark, {layer = 3})
    end
    if self.thread then
        local color = m.color(Spider.colors.silk):withAlpha(0.7 * self.thread.life)
        graphics2d.drawLine(self.thread.x, self.thread.y, self.crawler.x, self.crawler.y, 1.5, color, {layer = 4})
    end
    for _, leg in ipairs(self.legs) do
        local near = leg.layout.side == 'near'
        leg:draw(near and Spider.colors.near or Spider.colors.far, near and 6 or 3)
        if self.showGuides then
            graphics2d.drawCircle(leg.restX, leg.restY, 4, '#FF6FDCA0', {layer = 8})
            graphics2d.drawCircle(leg.poleX, leg.poleY, 3, '#FF8FB0FF', {layer = 8})
        end
    end
    self:drawBody()
end

return Spider
