-- A line of dominoes that stand along a curved valley and climb a flight of steps, falling one after the other from a single push, with the time of the chain reaction.
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local input = require('haylen.input')
local m = require('haylen.math')
local physics2d = require('haylen.physics2d')
local ui = require('haylen.ui')

local parts = require('categories.physics.parts')
local PhysicsTest = require('categories.physics.physics-test')

local Dominoes = haylen.class('Dominoes', PhysicsTest)

Dominoes.actions = {
    {name = 'push', type = 'button', bindings = {'key:space', 'button:north'}},
}

local kWidth, kHeight = 14, 64
local kFloor = 300
local kValley = {-500, 200, 30}
local kStairs = {x = 220, run = 40, rise = 16, count = 6}
local kPush = 120

function Dominoes:enter()
    self:frame{
        hint = 'Push the first domino, or click or tap any domino to push it. Space or the north button pushes the first one. R or the X button stands them up again.',
        controls = {
            ui.button{id = 'push', text = 'Push the first domino', onClick = function() self:push(self.dominoes[1].body) end},
            ui.button{id = 'reset', text = 'Stand them up again', onClick = function() self:build() end},
        },
        actions = Dominoes.actions,
        focus = 'push',
    }
    self:build()
end

-- Returns the height of the valley floor and the angle of its slope at `x`.
function Dominoes.valley(x)
    local from, to, depth = kValley[1], kValley[2], kValley[3]
    local phase = math.pi * (x - from) / (to - from)
    return kFloor + depth * math.sin(phase), math.atan(depth * math.pi / (to - from) * math.cos(phase))
end

function Dominoes:build()
    self.world = physics2d.newWorld()
    local top = kFloor - kStairs.rise * kStairs.count
    local stairsEnd = kStairs.x + kStairs.run * kStairs.count
    local profile = {{-790, kFloor}}
    for x = kValley[1], kValley[2], 35 do
        profile[#profile + 1] = {x, (Dominoes.valley(x))}
    end
    for step = 1, kStairs.count do
        local x = kStairs.x + (step - 1) * kStairs.run
        profile[#profile + 1] = {x, kFloor - (step - 1) * kStairs.rise}
        profile[#profile + 1] = {x, kFloor - step * kStairs.rise}
    end
    profile[#profile + 1] = {790, top}
    self.ground = self.world:createBody({type = 'static'})
    parts.chain(self.ground, profile, false, {friction = 0.8})
    local fill = {table.unpack(profile)}
    fill[#fill + 1] = {790, 430}
    fill[#fill + 1] = {-790, 430}
    parts.outline(self.ground, fill)
    parts.paint(self.ground, '#FF5D6B7A')

    self.dominoes = {}
    for x = -760, -520, 40 do
        self:stand(x, kFloor, 0)
    end
    for x = -480, 180, 38 do
        local y, angle = Dominoes.valley(x)
        self:stand(x, y, angle)
    end
    self:stand(205, kFloor, 0)
    for step = 1, kStairs.count do
        self:stand(kStairs.x + (step - 0.5) * kStairs.run, kFloor - step * kStairs.rise, 0)
    end
    for x = stairsEnd + 40, 740, 40 do
        self:stand(x, top, 0)
    end
    self.clock, self.running, self.finished = 0, false, false
end

-- Stands a domino on the ground point `x`, `y`, square to a slope of `angle`.
function Dominoes:stand(x, y, angle)
    local lift = kHeight / 2 + 0.5
    local body = self.world:createBody({x = x + math.sin(angle) * lift, y = y - math.cos(angle) * lift, rotation = angle})
    parts.box(body, kWidth, kHeight, {density = 2, friction = 0.6})
    parts.paint(body, m.fromHsv(#self.dominoes * 0.025 % 1, 0.55, 0.95))
    body.data.domino = true
    self.dominoes[#self.dominoes + 1] = {body = body, rotation = angle}
end

-- Pushes a domino to the right at its top, which starts the clock of the chain.
function Dominoes:push(body)
    local cos, sin = math.cos(body.rotation), math.sin(body.rotation)
    body:applyImpulse(body.mass * kPush * cos, body.mass * kPush * sin, body.x + sin * kHeight / 2, body.y - cos * kHeight / 2)
    if not self.running and not self.finished then
        self.running, self.clock = true, 0
    end
end

function Dominoes:fallen()
    local count = 0
    for _, domino in ipairs(self.dominoes) do
        count = count + (math.abs(domino.body.rotation - domino.rotation) > 0.5 and 1 or 0)
    end
    return count
end

function Dominoes:update(dt)
    Dominoes.super.update(self, dt)
    if input.pressed('reset') then
        self:build()
    end
    if input.pressed('push') then
        self:push(self.dominoes[1].body)
    end
    local pointer = self.pointer
    if pointer.pressed and not ui.usingPointer() then
        for _, shape in ipairs(self.world:pick(self.camera, pointer.x, pointer.y, {radius = 20})) do
            if shape.body.data and shape.body.data.domino then
                self:push(shape.body)
                break
            end
        end
    end
    local fallen = self:fallen()
    local state = self.running and 'running' or self.finished and 'finished' or 'waiting for a push'
    self:status(string.format('Fallen %d of %d, chain time %.2f seconds, %s, step %.2f ms', fallen, #self.dominoes, self.clock, state, self:stepTime()))
end

-- The clock stops once every domino fell or everything came to rest.
function Dominoes:fixedUpdate(step)
    self:simulate(self.world, step)
    if self.running then
        self.clock = self.clock + step
        if self:fallen() == #self.dominoes or self.world.awakeBodyCount == 0 then
            self.running, self.finished = false, true
        end
    end
end

function Dominoes:draw(area)
    parts.draw(self.ground)
    for _, domino in ipairs(self.dominoes) do
        parts.draw(domino.body, {layer = 1})
    end
    local text = self.finished and string.format('The chain took %.2f seconds', self.clock) or string.format('%.2f seconds', self.clock)
    graphics2d.drawText(nil, text, 0, -300, {size = 48, color = '#FFFFD54F', anchor = {0.5, 0.5}})
end

return Dominoes
