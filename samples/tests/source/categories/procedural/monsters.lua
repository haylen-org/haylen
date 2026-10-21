-- Monsters: monsters made from a seed and seen from the side. Each takes a wobbling body, a color, one to five eyes that follow the pointer and blink, a mouth with teeth, horns, spikes, arms, and either legs that walk the ground in a gait or tentacles that hang and sway while it floats. A click or a tap on a monster makes it roar.
local animation2d = require('haylen.animation2d')
local collections = require('haylen.collections')
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local input = require('haylen.input')
local m = require('haylen.math')
local ui = require('haylen.ui')

local AlgorithmTest = require('categories.algorithms.algorithm-test')
local Leg = require('categories.procedural.leg')
local names = require('categories.procedural.names')

local Monsters = haylen.class('Monsters', AlgorithmTest)

Monsters.code = [[
local monster = {eyes = random:weightedIndex({2, 5, 2, 1, 1}), horns = random:chance(0.5), floats = random:chance(0.3)}
local radius = monster.radius * (1 + wave[1] * math.sin(3 * angle + wave[2]))  -- A wobbling outline.
tentacle:reach(rootX + math.sin(time * 2 + index) * 30, rootY + tentacle.length * 0.9)  -- Hangs and sways.
pupilX, pupilY = eyeX + clampLength(pointer - eye, eye.radius * 0.4)  -- Every eye watches the pointer.]]

Monsters.ground = 210
Monsters.outline = '#FF1E1B33'
Monsters.bone = '#FFF1E6CC'
Monsters.horn = '#FFFFC93C'
Monsters.view = {1300, 620}
-- The ground, the hills and the sky reach past what any screen shows.
Monsters.reach = 1400

function Monsters:enter()
    self.seed = 2
    self.count = 4
    self:build()
    self:frame{
        view = Monsters.view,
        code = Monsters.code,
        hint = 'Every eye watches the pointer. Click or tap a monster to make it roar. R or the X button makes new monsters.',
        controls = {
            ui.formField{label = 'Monsters', ui.slider{id = 'count', min = 2, max = 7, step = 1, value = self.count, showValue = true, decimals = 0, onChange = function(event)
                self.count = math.tointeger(event.value)
                self:build()
            end}},
            ui.button{id = 'monsters', text = 'New monsters', variant = 'primary', onClick = function()
                self.seed = self.seed + 1
                self:build()
            end},
        },
    }
end

function Monsters:build()
    local random = m.random(self.seed)
    self.monsters = {}
    -- Every third monster floats, so every seed shows monsters that walk and monsters that float on tentacles.
    for index = 1, self.count do
        local x = (index - (self.count + 1) / 2) * (1150 / self.count)
        self.monsters[index] = Monsters.make(random, x, index % 3 == 2)
    end
    local reach = Monsters.reach
    self.ranges = {}
    for layer, height in ipairs({{170, 260}, {70, 150}}) do
        local hills = {{-reach, Monsters.ground}}
        for index = 0, 28 do
            hills[#hills + 1] = {-reach + index * reach * 2 / 28, Monsters.ground - random:range(height[1], height[2]) / layer}
        end
        hills[#hills + 1] = {reach, Monsters.ground}
        self.ranges[layer] = hills
    end
    self.stars = {}
    for _ = 1, 120 do
        self.stars[#self.stars + 1] = {random:range(-reach, reach), random:range(-560, 40), random:range(1, 2.6)}
    end
    self.roars = 0
end

-- Makes one monster standing at `x`, or floating when `floats` is set.
function Monsters.make(random, x, floats)
    local monster = {x = x, home = x, name = names.person(random), facing = random:chance(0.5) and 1 or -1}
    monster.width, monster.height = random:range(85, 125), random:range(72, 110)
    monster.hue = random:nextFloat()
    monster.color = m.fromHsv(monster.hue, random:range(0.45, 0.7), random:range(0.7, 0.9))
    monster.belly = monster.color:lerp(m.color('#FFFFF4E0'), 0.45)
    monster.waves = {}
    for order = 3, 6 do
        monster.waves[#monster.waves + 1] = {order, random:range(0.02, 0.08), random:range(0, math.pi * 2)}
    end
    monster.floats = floats
    monster.speed = random:range(30, 70)
    monster.phase = random:range(0, math.pi * 2)
    monster.roar = 0

    monster.eyes = {}
    local count = random:weightedIndex({2, 5, 2, 1, 1})
    for index = 1, count do
        local spread = count == 1 and 0 or (index - 1) / (count - 1) - 0.5
        monster.eyes[index] = {x = spread * monster.width * 1.1, y = -monster.height * random:range(0.25, 0.5), radius = random:range(13, 27), blinkIn = random:range(1, 5)}
    end
    monster.mouth = {width = monster.width * random:range(0.75, 1.05), teeth = random:integer(3, 6), fangs = random:chance(0.5)}
    monster.horns = random:chance(0.5) and random:range(25, 50) or nil
    monster.spikes = random:chance(0.4) and random:integer(5, 9) or 0

    monster.arms = {}
    if random:chance(0.5) then
        for side = -1, 1, 2 do
            monster.arms[#monster.arms + 1] = Leg({x = x, y = 0, lengths = {48, 42}, widths = {18, 15, 11}, color = monster.color, layer = 4})
            monster.arms[#monster.arms].side = side
        end
    end
    monster.legs, monster.tentacles = {}, {}
    if monster.floats then
        monster.altitude = random:range(250, 300)
        for index = 1, random:integer(3, 6) do
            local tentacle = animation2d.newChain({segments = 7, length = random:range(17, 24), angle = math.pi / 2, maxBend = 0.5})
            monster.tentacles[index] = {chain = tentacle, reach = {iterations = 6}, buffer = collections.newFloatBuffer(16), stroke = {widths = {20, 18, 15, 12, 10, 7, 5, 3}, color = monster.color, join = 'round', cap = 'round', layer = 2}, outline = {widths = {26, 24, 21, 18, 16, 13, 11, 8}, color = Monsters.outline, join = 'round', cap = 'round', layer = 2}}
        end
    else
        local pairs = random:chance(0.6) and 1 or 2
        monster.legLength = random:range(75, 115)
        for pair = 1, pairs do
            for side = -1, 1, 2 do
                local leg = Leg({x = x, y = 0, lengths = {monster.legLength * 0.5, monster.legLength * 0.55}, group = (pair + (side > 0 and 1 or 0)) % 2 + 1, stepTime = 0.22, lift = 22, widths = {26, 22, 16}, color = monster.color:lerp(m.color(Monsters.outline), 0.2), layer = 2})
                leg.offset = side * monster.width * (pairs == 1 and 0.4 or (pair == 1 and 0.6 or 0.2))
                monster.legs[#monster.legs + 1] = leg
            end
        end
        monster.altitude = monster.legLength * 0.8 + monster.height * 0.7
    end
    Monsters.place(monster, 0, 0, true)
    return monster
end

-- Returns the radius of the body outline at an angle.
function Monsters.radius(monster, angle)
    local scale = 1
    for _, wave in ipairs(monster.waves) do
        scale = scale + wave[2] * math.sin(wave[1] * angle + wave[3])
    end
    return scale
end

-- Places the body over the ground and the hips, rest points and poles of its limbs.
function Monsters.place(monster, time, dt, settle)
    local bob = math.sin(time * (monster.floats and 1.6 or 6) + monster.phase) * (monster.floats and 12 or 3)
    local shake = monster.roar > 0 and math.sin(time * 60) * 4 or 0
    monster.bodyX, monster.bodyY = monster.x + shake, Monsters.ground - monster.altitude + bob
    for _, leg in ipairs(monster.legs) do
        leg.hipX, leg.hipY = monster.bodyX + leg.offset * 0.7, monster.bodyY + monster.height * 0.6
        leg.restX, leg.restY = monster.bodyX + leg.offset, Monsters.ground
        leg.poleX, leg.poleY = leg.hipX + monster.facing * 60, leg.hipY + 10
        if settle then
            leg:place(leg.restX, leg.restY)
        end
    end
    for _, arm in ipairs(monster.arms) do
        local side = arm.side
        arm.hipX, arm.hipY = monster.bodyX + side * monster.width * 0.8, monster.bodyY + monster.height * 0.1
        local raised = monster.roar > 0 and -1 or 1
        arm:place(arm.hipX + side * 30, arm.hipY + raised * 50 + math.sin(time * 3 + side + monster.phase) * 10)
        arm.poleX, arm.poleY = arm.hipX + side * 60, arm.hipY - 10 * raised
    end
end

function Monsters:update(dt)
    Monsters.super.update(self, dt)
    if not self.stage then
        return
    end
    if input.pressed('reset') then
        self.seed = self.seed + 1
        self:build()
    end
    local pointer = self.pointer
    local time = haylen.elapsed()
    local walking = 0
    for _, monster in ipairs(self.monsters) do
        if pointer.pressed and math.abs(pointer.worldX - monster.bodyX) < monster.width and math.abs(pointer.worldY - monster.bodyY) < monster.height then
            monster.roar = 1.2
            self.roars = self.roars + 1
            self:log('%s roars.', monster.name)
        end
        monster.roar = math.max(0, monster.roar - dt)

        -- A monster paces around its place and turns at the ends, and stands still while it roars.
        if monster.roar == 0 then
            monster.x = monster.x + monster.facing * monster.speed * dt
            if math.abs(monster.x - monster.home) > 60 then
                monster.facing = monster.x > monster.home and -1 or 1
            end
            walking = walking + 1
        end
        Monsters.place(monster, time, dt, false)
        Leg.gait(monster.legs, 40, monster.facing * monster.speed * 0.25, 0)
        for _, leg in ipairs(monster.legs) do
            leg:update(dt, leg.hipX, leg.hipY, 0, -1, leg.poleX, leg.poleY)
        end
        for _, arm in ipairs(monster.arms) do
            arm:update(dt, arm.hipX, arm.hipY, 0, 0, arm.poleX, arm.poleY)
        end
        for index, tentacle in ipairs(monster.tentacles) do
            local spread = (index - (#monster.tentacles + 1) / 2) / #monster.tentacles
            local rootX, rootY = monster.bodyX + spread * monster.width * 1.2, monster.bodyY + monster.height * 0.7
            tentacle.chain.x, tentacle.chain.y = rootX, rootY
            tentacle.chain:reach(rootX + math.sin(time * 2 + index + monster.phase) * 34 - monster.facing * 20, rootY + tentacle.chain.length * 0.85, tentacle.reach)
        end
        for _, eye in ipairs(monster.eyes) do
            eye.blinkIn = eye.blinkIn - dt
            if eye.blinkIn < -0.12 then
                eye.blinkIn = 2 + (eye.radius * 1.7 + time) % 4
            end
        end
    end
    local roars = self.roars == 0 and 'Touch one to make it roar.' or string.format('They roared %d %s.', self.roars, self.roars == 1 and 'time' or 'times')
    self:status(string.format('%d monsters from the seed %d, %d of them pacing. %s', #self.monsters, self.seed, walking, roars))
end

-- Returns a flat list of the points of a closed outline, for its stroke.
function Monsters.flatten(points)
    local flat = {}
    for _, point in ipairs(points) do
        flat[#flat + 1] = point[1]
        flat[#flat + 1] = point[2]
    end
    return flat
end

-- The body draws with the outline of the art style around everything: spikes and horns behind its edge, the wobbling body with its belly and a soft highlight, the eyes and a big mouth with teeth on both jaws and a tongue.
function Monsters.drawBody(monster, time)
    local x, y = monster.bodyX, monster.bodyY
    local roaring = monster.roar > 0
    local scale = roaring and 1.06 or 1
    local outline = {}
    for index = 0, 47 do
        local angle = index / 48 * math.pi * 2
        local radius = Monsters.radius(monster, angle)
        outline[#outline + 1] = {x + math.cos(angle) * monster.width * radius * scale, y + math.sin(angle) * monster.height * radius * scale}
    end
    if monster.horns then
        for side = -1, 1, 2 do
            local baseX, baseY = x + side * monster.width * 0.42, y - monster.height * 0.78
            local horn = {{baseX - 13, baseY + 8}, {baseX + 13, baseY + 8}, {baseX + side * monster.horns * 0.75, baseY - monster.horns}}
            graphics2d.drawStroke(Monsters.flatten(horn), {width = 7, color = Monsters.outline, join = 'round', closed = true, layer = 2})
            graphics2d.drawPolygon(horn, Monsters.horn, {layer = 2})
        end
    end
    for spike = 1, monster.spikes do
        local angle = math.pi * (1.2 + (spike - 1) / math.max(1, monster.spikes - 1) * 0.6)
        local radius = Monsters.radius(monster, angle)
        local baseX, baseY = x + math.cos(angle) * monster.width * radius * 0.92, y + math.sin(angle) * monster.height * radius * 0.92
        local tipX, tipY = x + math.cos(angle) * (monster.width * radius + 24), y + math.sin(angle) * (monster.height * radius + 24)
        local across = {-math.sin(angle) * 10, math.cos(angle) * 10}
        local points = {{baseX + across[1], baseY + across[2]}, {baseX - across[1], baseY - across[2]}, {tipX, tipY}}
        graphics2d.drawStroke(Monsters.flatten(points), {width = 6, color = Monsters.outline, join = 'round', closed = true, layer = 2})
        graphics2d.drawPolygon(points, monster.color:lerp(m.color(Monsters.outline), 0.35), {layer = 2})
    end
    graphics2d.drawStroke(Monsters.flatten(outline), {width = 8, color = Monsters.outline, join = 'round', closed = true, layer = 3})
    graphics2d.drawPolygon(outline, monster.color, {layer = 3})
    graphics2d.drawEllipse(x + monster.facing * monster.width * 0.1, y + monster.height * 0.35, monster.width * 0.62, monster.height * 0.46, monster.belly, {layer = 3})
    graphics2d.drawShape({x - monster.width * 0.62, y - monster.height * 0.78, monster.width * 0.5, monster.height * 0.22}, {kind = 'ellipse', rotation = -0.35, color = '#40FFFFFF', layer = 3})

    local faceX = x + monster.facing * monster.width * 0.12
    for _, eye in ipairs(monster.eyes) do
        local eyeX, eyeY = faceX + eye.x, y + eye.y
        local radius = eye.radius * (roaring and 1.25 or 1)
        graphics2d.drawCircle(eyeX, eyeY, radius + 3.5, Monsters.outline, {layer = 4})
        if eye.blinkIn < 0 then
            graphics2d.drawCircle(eyeX, eyeY, radius, monster.color, {layer = 4})
            graphics2d.drawLine(eyeX - radius, eyeY, eyeX + radius, eyeY, 3.5, Monsters.outline, {layer = 4})
        else
            graphics2d.drawCircle(eyeX, eyeY, radius, '#FFFFFFFF', {layer = 4})
            local lookX, lookY = (monster.lookX or eyeX) - eyeX, (monster.lookY or eyeY) - eyeY
            local length = math.max(1, math.sqrt(lookX * lookX + lookY * lookY))
            local reach = math.min(length, radius * 0.45) / length
            graphics2d.drawCircle(eyeX + lookX * reach, eyeY + lookY * reach, radius * 0.5, Monsters.outline, {layer = 4})
            graphics2d.drawCircle(eyeX + lookX * reach + radius * 0.15, eyeY + lookY * reach - radius * 0.15, radius * 0.15, '#FFFFFFFF', {layer = 4})
        end
    end

    local mouth = monster.mouth
    local open = roaring and 56 or 26 + math.sin(time * 2 + monster.phase) * 5
    local mouthY = y + monster.height * 0.32
    local mouthRect = {faceX - mouth.width / 2, mouthY - open / 2, mouth.width, open}
    graphics2d.drawShape(mouthRect, {kind = 'ellipse', color = '#FF4A1424', borderWidth = 3.5, borderColor = Monsters.outline, layer = 4})
    graphics2d.drawShape({faceX - mouth.width * 0.24, mouthY + open * 0.08, mouth.width * 0.48, open * 0.36}, {kind = 'ellipse', color = '#FFE8667A', layer = 4})
    for tooth = 1, mouth.teeth do
        local toothX = faceX - mouth.width * 0.36 + (tooth - 0.5) / mouth.teeth * mouth.width * 0.72
        local length = (mouth.fangs and (tooth == 1 or tooth == mouth.teeth)) and open * 0.55 or open * 0.3
        graphics2d.drawPolygon({{toothX - 7, mouthY - open / 2 + 3}, {toothX + 7, mouthY - open / 2 + 3}, {toothX, mouthY - open / 2 + 3 + length}}, Monsters.bone, {layer = 4})
    end
    for tooth = 1, math.max(1, mouth.teeth - 2) do
        local toothX = faceX - mouth.width * 0.22 + (tooth - 0.5) / math.max(1, mouth.teeth - 2) * mouth.width * 0.44
        graphics2d.drawPolygon({{toothX - 6, mouthY + open / 2 - 3}, {toothX + 6, mouthY + open / 2 - 3}, {toothX, mouthY + open / 2 - 3 - open * 0.24}}, Monsters.bone, {layer = 4})
    end
    if roaring then
        graphics2d.drawText(nil, 'Roar!', x, y - monster.height - 44, {size = 40, color = '#FFFFE08A', outlineWidth = 4, outlineColor = Monsters.outline, anchor = {0.5, 1}, layer = 6})
    end
end

-- A night over two ranges of hills, the far one lighter than the near one, and ground that fill the whole canvas.
function Monsters:draw(area)
    local bounds = graphics2d.canvasBounds()
    graphics2d.drawRect(bounds, '#FF161B2A', {layer = -3})
    for _, star in ipairs(self.stars) do
        graphics2d.drawCircle(star[1], star[2], star[3], '#B3E8EAF2', {layer = -2})
    end
    -- A crescent moon in its glow: the disc of the moon without the part a smaller disc covers.
    graphics2d.drawCircle(430, -190, 80, '#1AF4EBD0', {layer = -2})
    graphics2d.pushMask(function()
        graphics2d.drawCircle(450, -202, 46, '#FFFFFFFF')
    end, {mode = 'outside', layer = -2})
    graphics2d.drawCircle(430, -190, 52, '#FFF4EBD0', {layer = -2})
    graphics2d.popMask()
    graphics2d.drawPolygon(self.ranges[1], '#FF2C3552', {layer = -1})
    graphics2d.drawPolygon(self.ranges[2], '#FF222A40', {layer = -1})
    graphics2d.drawRect({bounds.x, Monsters.ground, bounds.width, bounds.y + bounds.height - Monsters.ground}, '#FF2C2A3C', {layer = -1})
    graphics2d.drawRect({bounds.x, Monsters.ground, bounds.width, 5}, '#FF4A4560', {layer = -1})
    local time = haylen.elapsed()
    for _, monster in ipairs(self.monsters) do
        monster.lookX, monster.lookY = self.pointer.worldX, self.pointer.worldY
        graphics2d.drawEllipse(monster.bodyX, Monsters.ground + 8, monster.width * 0.9, 10, '#55000000', {layer = 1})
        for _, leg in ipairs(monster.legs) do
            leg:draw()
        end
        for _, tentacle in ipairs(monster.tentacles) do
            tentacle.chain:writePoints(tentacle.buffer)
            graphics2d.drawStroke(tentacle.buffer, tentacle.outline)
            graphics2d.drawStroke(tentacle.buffer, tentacle.stroke)
        end
        for _, arm in ipairs(monster.arms) do
            arm:draw()
            graphics2d.drawCircle(arm.footX, arm.footY, 12, Monsters.outline, {layer = 4})
            graphics2d.drawCircle(arm.footX, arm.footY, 9, monster.color:lerp(m.color(Monsters.outline), 0.3), {layer = 4})
        end
        Monsters.drawBody(monster, time)
        graphics2d.drawText(nil, monster.name, monster.home, Monsters.ground + 30, {size = 24, color = '#FFE8EAF2', anchor = {0.5, 0}})
    end
end

return Monsters
