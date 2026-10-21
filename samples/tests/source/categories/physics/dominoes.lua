-- A line of ivory dominoes that stand along a grassy valley and climb a flight of stone steps, falling one after the other from a single push, with the time of the chain reaction. The first domino falls by itself when the test opens.
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local input = require('haylen.input')
local physics2d = require('haylen.physics2d')
local timer = require('haylen.timer')
local ui = require('haylen.ui')

local Look = require('categories.physics.look')
local PhysicsTest = require('categories.physics.physics-test')
local Scenery = require('categories.physics.scenery')

local Dominoes = haylen.class('Dominoes', PhysicsTest)

Dominoes.code = [[
domino:addBox(16, 68, {density = 2, friction = 0.6})
ground:addChain(profile, false, {friction = 0.8})  -- The valley and the steps, without seams.
domino:applyImpulse(domino.mass * 120, 0, domino.x, domino.y - 34)  -- A push at the top.
local done = world.awakeBodyCount == 0  -- The chain came to rest.]]

Dominoes.actions = {
    {name = 'push', type = 'button', bindings = {'key:space', 'button:north'}},
}

Dominoes.width, Dominoes.height = 16, 68
Dominoes.floor = 300
-- The valley runs from its first to its last x and dips this deep in its middle.
Dominoes.valleyFrom, Dominoes.valleyTo, Dominoes.valleyDepth = -440, 160, 34
Dominoes.stairs = {x = 200, run = 40, rise = 16, count = 6}
Dominoes.edge = 790
Dominoes.push = 120
Dominoes.firstPush = 1
Dominoes.ivory = Look.tint('yellow', '#FFFBF4E2', '#FFD9CCB0', '#FFFFFFFF')
-- The pips of each half of a domino, as offsets along its height from the middle of the half, by their count.
Dominoes.pips = {{0}, {-9, 9}, {-11, 0, 11}}

function Dominoes:enter()
    self:frame{
        code = Dominoes.code,
        hint = 'Push the first domino, or click or tap any domino to push it. Space or the north button pushes the first one. R or the X button stands them up again.',
        controls = {
            ui.button{id = 'push', text = 'Push the first domino', onClick = function() self:topple(self.dominoes[1].body) end},
            ui.button{id = 'reset', text = 'Stand them up again', onClick = function() self:build() end},
        },
        actions = Dominoes.actions,
        focus = 'push',
        view = {1420, 620},
    }
    self.camera.position = {0, 100}
    self:build()
    timer.after(Dominoes.firstPush, function()
        if not self.running and not self.finished then
            self:topple(self.dominoes[1].body)
        end
    end, {owner = self})
end

-- Returns the height of the valley floor and the angle of its slope at `x`.
function Dominoes.valley(x)
    local from, to, depth = Dominoes.valleyFrom, Dominoes.valleyTo, Dominoes.valleyDepth
    local phase = math.pi * (x - from) / (to - from)
    return Dominoes.floor + depth * math.sin(phase), math.atan(depth * math.pi / (to - from) * math.cos(phase))
end

function Dominoes:build()
    self.world = physics2d.newWorld()
    local floor, stairs = Dominoes.floor, Dominoes.stairs
    local top = floor - stairs.rise * stairs.count
    local stairsEnd = stairs.x + stairs.run * stairs.count

    -- The grass runs along the valley to the foot of the steps, and the steps and the landing above them are stone blocks on it.
    local lawn = {{-Dominoes.edge, floor}}
    for x = Dominoes.valleyFrom, Dominoes.valleyTo, 30 do
        lawn[#lawn + 1] = {x, (Dominoes.valley(x))}
    end
    lawn[#lawn + 1] = {Dominoes.edge, floor}
    self.lawn = Scenery.newTerrain(lawn)
    local profile = {}
    for index = 1, #lawn - 1 do
        profile[index] = lawn[index]
    end
    for step = 1, stairs.count do
        local x = stairs.x + (step - 1) * stairs.run
        profile[#profile + 1] = {x, floor - (step - 1) * stairs.rise}
        profile[#profile + 1] = {x, floor - step * stairs.rise}
    end
    profile[#profile + 1] = {Dominoes.edge, top}
    self.ground = self.world:createBody({type = 'static'})
    self.ground:addChain(profile, false, {friction = 0.8})

    self.dominoes = {}
    for x = -680, -470, 38 do
        self:stand(x, floor, 0)
    end
    for x = -440, 150, 36 do
        local y, angle = Dominoes.valley(x)
        self:stand(x, y, angle)
    end
    self:stand(180, floor, 0)
    for step = 1, stairs.count do
        self:stand(stairs.x + (step - 0.5) * stairs.run, floor - step * stairs.rise, 0)
    end
    for x = stairsEnd + 38, 690, 38 do
        self:stand(x, top, 0)
    end
    self.clock, self.running, self.finished = 0, false, false
end

-- Stands a domino on the ground point `x`, `y`, square to a slope of `angle`, with the pips of its two halves.
function Dominoes:stand(x, y, angle)
    local lift = Dominoes.height / 2 + 0.5
    local body = self.world:createBody({x = x + math.sin(angle) * lift, y = y - math.cos(angle) * lift, rotation = angle})
    body:addBox(Dominoes.width, Dominoes.height, {density = 2, friction = 0.6})
    Look.dress(body, Dominoes.ivory, {radius = 3})
    local index = #self.dominoes
    body.data.domino = true
    self.dominoes[index + 1] = {body = body, rotation = angle, top = index * 7 % 3 + 1, bottom = (index * 5 + 1) % 3 + 1}
end

-- Pushes a domino to the right at its top, which starts the clock of the chain.
function Dominoes:topple(body)
    local cos, sin = math.cos(body.rotation), math.sin(body.rotation)
    local half = Dominoes.height / 2
    body:applyImpulse(body.mass * Dominoes.push * cos, body.mass * Dominoes.push * sin, body.x + sin * half, body.y - cos * half)
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
        self:topple(self.dominoes[1].body)
    end
    local pointer = self.pointer
    if pointer.pressed and not ui.usingPointer() then
        for _, shape in ipairs(self.world:pick(self.camera, pointer.x, pointer.y, {radius = 20})) do
            if shape.body.data and shape.body.data.domino then
                self:topple(shape.body)
                break
            end
        end
    end
    local fallen, total = self:fallen(), #self.dominoes
    if self.finished then
        self:status(string.format('The chain toppled %d of the %d dominoes in %.2f seconds.', fallen, total, self.clock))
    elseif self.running then
        self:status(string.format('%d of the %d dominoes have fallen, %.2f seconds into the chain.', fallen, total, self.clock))
    else
        self:status(string.format('%d dominoes stand and wait for a push.', total))
    end
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

-- Draws the pips of one half of a domino, `count` of them in a column around the point `along` its height from its center.
function Dominoes:drawPips(body, count, along, cos, sin)
    for _, offset in ipairs(Dominoes.pips[count]) do
        local y = along + offset
        graphics2d.drawCircle(body.x - y * sin, body.y + y * cos, 2.6, '#FF2F2B45', {layer = 2})
    end
end

function Dominoes:draw(area)
    local floor, stairs = Dominoes.floor, Dominoes.stairs
    Scenery.sky(self.camera, floor)
    Scenery.drawTerrain(self.lawn)
    for step = 1, stairs.count do
        local height = step * stairs.rise
        Look.block(stairs.x + (step - 0.5) * stairs.run, floor - height / 2, stairs.run, height, 'stone', 0)
    end
    local stairsEnd = stairs.x + stairs.run * stairs.count
    local height = stairs.rise * stairs.count
    Look.block((stairsEnd + Dominoes.edge) / 2, floor - height / 2, Dominoes.edge - stairsEnd, height, 'stone', 0)

    local unit = Look.pixel
    for _, domino in ipairs(self.dominoes) do
        local body = domino.body
        Look.draw(body, {layer = 1})
        local cos, sin = math.cos(body.rotation), math.sin(body.rotation)
        local half = Dominoes.width / 2 - 3
        graphics2d.drawLine(body.x - half * cos, body.y - half * sin, body.x + half * cos, body.y + half * sin, 2 * unit, '#FF8C8270', {layer = 2})
        self:drawPips(body, domino.top, -Dominoes.height / 4, cos, sin)
        self:drawPips(body, domino.bottom, Dominoes.height / 4, cos, sin)
    end
    local text = self.finished and string.format('The chain took %.2f seconds', self.clock) or string.format('%.2f seconds', self.clock)
    self:label(text, 0, -110, {size = 30})
end

return Dominoes
