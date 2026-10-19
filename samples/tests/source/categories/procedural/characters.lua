-- Characters: a crowd of people made from a seed and drawn with shapes alone. Each person takes a build, a skin tone, a hair style and color, a face, clothes, accessories and a name from the generator, breathes and blinks, and has arms that are chains of two segments, which hang at rest and reach up to wave when the pointer comes near.
local animation2d = require('haylen.animation2d')
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local input = require('haylen.input')
local m = require('haylen.math')
local ui = require('haylen.ui')

local AlgorithmTest = require('categories.algorithms.algorithm-test')
local names = require('categories.procedural.names')

local Characters = haylen.class('Characters', AlgorithmTest)

Characters.code = [[
local person = {height = random:range(0.85, 1.15), skin = pick(skins), hair = pick(hairStyles), outfit = pick(outfits)}
local arm = animation2d.newChain({x = shoulderX, y = shoulderY, lengths = {50, 48}})
arm:reach(handX, handY, {pole = {shoulderX + side * 80, shoulderY + 40}})  -- The elbow bends outward.
if near(pointer) then arm:reach(shoulderX + 70, shoulderY - 70 + math.sin(time * 10) * 14) end  -- Waves.]]

Characters.outline = '#FF2B2233'
Characters.skins = {'#FFFBD9C0', '#FFF1C3A0', '#FFE0A57E', '#FFC68457', '#FF9C6440', '#FF7A4A2E', '#FF5C3622'}
Characters.hairColors = {'#FF2A2120', '#FF5A3826', '#FF8A4E2A', '#FFD9B060', '#FFB8582E', '#FF9A9AA0', '#FFE8E4DC', '#FF5B7BE0', '#FFE06AA8'}
Characters.hairWeights = {14, 14, 10, 8, 6, 4, 2, 1, 1}
Characters.cloth = {'#FFE5534B', '#FF3C9BE0', '#FF35B37E', '#FFF2B23A', '#FFA87FE0', '#FF2E3A55', '#FFEFE6D8', '#FF7A5642', '#FF2B8C8C', '#FFE07A3A'}
Characters.hairStyles = {'short', 'long', 'bun', 'spiky', 'bald', 'ponytail', 'curly'}
Characters.tops = {'tee', 'sweater', 'jacket', 'dress'}
Characters.bottoms = {'pants', 'shorts', 'skirt'}
Characters.hats = {'cap', 'beanie', 'topHat', 'bow'}
Characters.columns, Characters.spacing, Characters.grounds = 5, 300, {-60, 360}

function Characters:enter()
    self.seed = 1
    self.wave = true
    self:build()
    self:frame{
        code = Characters.code,
        hint = 'Move the pointer near someone and they wave at it. Click or tap a person to make them hop. R or the X button makes a new crowd.',
        controls = {
            ui.checkbox{id = 'wave', text = 'Wave at the pointer', checked = true, onChange = function(event) self.wave = event.checked end},
            ui.button{id = 'crowd', text = 'New crowd', variant = 'primary', onClick = function() self:newCrowd() end},
        },
    }
end

function Characters:newCrowd()
    self.seed = self.seed + 1
    self:build()
end

function Characters:build()
    local random = m.random(self.seed)
    self.people = {}
    for index = 1, Characters.columns * #Characters.grounds do
        local column, row = (index - 1) % Characters.columns, (index - 1) // Characters.columns
        local x = (column - (Characters.columns - 1) / 2) * Characters.spacing
        self.people[index] = Characters.make(random, x, Characters.grounds[row + 1])
    end
    self:log('Crowd %d: %s.', self.seed, table.concat(self:listNames(), ', '))
end

function Characters:listNames()
    local list = {}
    for _, person in ipairs(self.people) do
        list[#list + 1] = person.name
    end
    return list
end

-- Makes one person standing with their feet at `x`, `ground`.
function Characters.make(random, x, ground)
    local pick = function(list) return list[random:integer(1, #list)] end
    local person = {x = x, ground = ground, name = names.person(random)}
    person.size = random:range(0.85, 1.12)
    person.build = random:range(0.85, 1.2)
    person.head = random:range(36, 42)
    person.skin = pick(Characters.skins)
    person.hairColor = Characters.hairColors[random:weightedIndex(Characters.hairWeights)]
    person.hair = pick(Characters.hairStyles)
    person.top = pick(Characters.tops)
    person.bottom = person.top == 'dress' and 'none' or pick(Characters.bottoms)
    person.shirt, person.jacket, person.legs, person.shoes = pick(Characters.cloth), pick(Characters.cloth), pick(Characters.cloth), pick({'#FF2A2120', '#FF5A3826', '#FFEFE6D8', '#FFE5534B'})
    person.hat = random:chance(0.45) and pick(Characters.hats) or nil
    person.glasses = random:chance(0.25)
    person.beard = person.hair ~= 'long' and random:chance(0.18)
    person.freckles = random:chance(0.2)
    person.blush = random:chance(0.35)
    person.smile = pick({'smile', 'grin', 'flat', 'open'})
    person.phase = random:range(0, math.pi * 2)
    person.blinkIn = random:range(1, 4)
    person.hop = 0
    person.arms = {}
    for side = -1, 1, 2 do
        person.arms[#person.arms + 1] = {side = side, chain = animation2d.newChain({lengths = {50 * person.size, 48 * person.size}, angle = math.pi / 2}), reach = {pole = {0, 0}}, upper = {0, 0, 0, 0}, lower = {0, 0, 0, 0}}
    end
    return person
end

-- Returns the place of the hips, the shoulders and the head of a person this frame.
function Characters.pose(person, time)
    local size = person.size
    local breath = math.sin(time * 2 + person.phase) * 2
    local feet = person.ground - person.hop
    local hip = feet - 110 * size
    local shoulder = hip - 92 * size + breath
    local head = shoulder - 14 - person.head * 0.9
    return feet, hip, shoulder, head
end

function Characters:update(dt)
    Characters.super.update(self, dt)
    if not self.stage then
        return
    end
    if input.pressed('reset') then
        self:newCrowd()
    end
    local pointer = self.pointer
    local time = haylen.elapsed()
    local hovered
    for _, person in ipairs(self.people) do
        local feet, hip, shoulder, head = Characters.pose(person, time)
        local over = math.abs(pointer.worldX - person.x) < 110 and pointer.worldY > head - 60 and pointer.worldY < person.ground
        hovered = over and person or hovered
        if pointer.pressed and over and person.hop == 0 then
            person.velocity = 420
            self:log('%s hops.', person.name)
        end
        if person.velocity then
            person.hop = person.hop + person.velocity * dt
            person.velocity = person.velocity - 1600 * dt
            if person.hop <= 0 then
                person.hop, person.velocity = 0, nil
            end
        end
        person.blinkIn = person.blinkIn - dt
        if person.blinkIn < -0.12 then
            person.blinkIn = 2 + (person.phase * 7 + time) % 3
        end

        local near = self.wave and math.abs(pointer.worldX - person.x) < 260 and math.abs(pointer.worldY - shoulder) < 260
        local width = 44 * person.build
        for _, arm in ipairs(person.arms) do
            local side = arm.side
            local shoulderX, shoulderY = person.x + side * (width - 6), shoulder + 16
            arm.chain.x, arm.chain.y = shoulderX, shoulderY
            local handX, handY, poleX, poleY
            local waving = near and (side > 0) == (pointer.worldX >= person.x)
            if waving then
                handX = shoulderX + side * 60 * person.size + math.sin(time * 10 + person.phase) * 16
                handY = shoulderY - 80 * person.size
                poleX, poleY = shoulderX + side * 90, shoulderY + 30
            else
                handX = shoulderX + side * (14 + math.sin(time * 1.6 + person.phase) * 4)
                handY = shoulderY + 92 * person.size
                poleX, poleY = shoulderX + side * 80, shoulderY + 40
            end
            arm.reach.pole[1], arm.reach.pole[2] = poleX, poleY
            arm.chain:reach(handX, handY, arm.reach)
        end
    end
    if hovered then
        self:status(string.format('%s: %s hair, %s, %s%s%s', hovered.name, hovered.hair, hovered.top, hovered.bottom == 'none' and 'no legwear' or hovered.bottom, hovered.hat and (', ' .. hovered.hat) or '', hovered.glasses and ', glasses' or ''))
    else
        self:status(string.format('Crowd %d of %d people, point at someone to read them', self.seed, #self.people))
    end
end

-- Draws a shape with the outline of the crowd.
function Characters.shape(rect, color, options)
    options = options or {}
    options.color, options.borderWidth, options.borderColor = color, options.borderWidth or 3, Characters.outline
    graphics2d.drawShape(rect, options)
end

function Characters.drawHairBack(person, x, head)
    local radius = person.head
    if person.hair == 'long' then
        Characters.shape({x - radius * 1.15, head - radius * 0.9, radius * 2.3, radius * 3.1}, person.hairColor, {radius = radius})
    elseif person.hair == 'ponytail' then
        Characters.shape({x + radius * 0.6, head - radius * 0.2, radius * 0.9, radius * 1.9}, person.hairColor, {kind = 'ellipse', rotation = -0.4})
    elseif person.hair == 'bun' then
        Characters.shape({x - radius * 0.5, head - radius * 1.85, radius, radius}, person.hairColor, {kind = 'ellipse'})
    end
end

function Characters.drawHairFront(person, x, head)
    local radius = person.head
    local color = person.hairColor
    if person.hair == 'short' or person.hair == 'long' or person.hair == 'bun' or person.hair == 'ponytail' then
        Characters.shape({x - radius * 1.06, head - radius * 1.12, radius * 2.12, radius * 1.3}, color, {kind = 'ellipse', startAngle = math.pi, sweep = math.pi, borderWidth = 0})
        Characters.shape({x - radius * 1.06, head - radius * 0.8, radius * 2.12, radius * 0.7}, color, {radius = {radius * 0.3, radius * 0.3, radius * 0.1, radius * 0.6}, borderWidth = 0})
    elseif person.hair == 'spiky' then
        local points = {}
        for index = 0, 8 do
            local angle = math.pi + index / 8 * math.pi
            local reach = radius * (index % 2 == 0 and 1.05 or 1.4)
            points[#points + 1] = {x + math.cos(angle) * reach, head + math.sin(angle) * reach}
        end
        points[#points + 1] = {x + radius, head - radius * 0.3}
        points[#points + 1] = {x - radius, head - radius * 0.3}
        graphics2d.drawPolygon(points, color)
    elseif person.hair == 'curly' then
        for index = 0, 6 do
            local angle = math.pi * 1.05 + index / 6 * math.pi * 0.9
            graphics2d.drawCircle(x + math.cos(angle) * radius * 0.95, head + math.sin(angle) * radius * 0.95, radius * 0.42, color)
        end
    elseif person.hair == 'bald' then
        Characters.shape({x - radius * 1.08, head - radius * 0.2, radius * 0.4, radius * 0.6}, color, {radius = 6, borderWidth = 0})
        Characters.shape({x + radius * 0.68, head - radius * 0.2, radius * 0.4, radius * 0.6}, color, {radius = 6, borderWidth = 0})
    end
end

function Characters.drawFace(person, x, head)
    local radius = person.head
    local eyeY = head + radius * 0.05
    local outline = Characters.outline
    for side = -1, 1, 2 do
        local eyeX = x + side * radius * 0.38
        if person.blinkIn < 0 then
            graphics2d.drawLine(eyeX - 5, eyeY, eyeX + 5, eyeY, 3, outline)
        else
            graphics2d.drawCircle(eyeX, eyeY, 4.5, outline)
            graphics2d.drawCircle(eyeX + 1.5, eyeY - 1.5, 1.5, '#FFFFFFFF')
        end
        graphics2d.drawLine(eyeX - 7, eyeY - 12 - side, eyeX + 7, eyeY - 12 + side, 3, outline)
        if person.blush then
            graphics2d.drawEllipse(x + side * radius * 0.6, head + radius * 0.4, 7, 4, '#66FF6A6A')
        end
        if person.glasses then
            graphics2d.drawRing(eyeX, eyeY, 10, 2.5, outline)
        end
    end
    if person.glasses then
        graphics2d.drawLine(x - radius * 0.38 + 10, eyeY, x + radius * 0.38 - 10, eyeY, 2.5, outline)
    end
    if person.freckles then
        for index = -2, 2 do
            graphics2d.drawCircle(x + index * 5, head + radius * 0.3 + (index % 2) * 3, 1.3, '#AA8A4E2A')
        end
    end
    graphics2d.drawEllipse(x, head + radius * 0.25, 4, 3, m.color(person.skin):lerp(m.color(outline), 0.25))
    if person.beard then
        Characters.shape({x - radius * 0.8, head + radius * 0.25, radius * 1.6, radius * 0.95}, person.hairColor, {kind = 'ellipse', startAngle = 0, sweep = math.pi, borderWidth = 0})
    end
    local mouthY = head + radius * 0.55
    if person.smile == 'smile' then
        graphics2d.drawArc(x, mouthY - 6, 9, 3, 0.4, math.pi - 0.4, outline)
    elseif person.smile == 'grin' then
        Characters.shape({x - 10, mouthY - 5, 20, 10}, '#FFFFFFFF', {kind = 'ellipse', startAngle = 0, sweep = math.pi, borderWidth = 2.5})
    elseif person.smile == 'open' then
        graphics2d.drawEllipse(x, mouthY, 5, 6, outline)
    else
        graphics2d.drawLine(x - 7, mouthY, x + 7, mouthY, 3, outline)
    end
end

function Characters.drawHat(person, x, head)
    local radius = person.head
    local top = head - radius
    if person.hat == 'cap' then
        Characters.shape({x - radius * 1.05, top - 6, radius * 2.1, radius * 1.2}, person.jacket, {kind = 'ellipse', startAngle = math.pi, sweep = math.pi})
        Characters.shape({x - radius * 0.2, top + radius * 0.42, radius * 1.6, 10}, person.jacket, {radius = 5})
    elseif person.hat == 'beanie' then
        Characters.shape({x - radius * 1.05, top - 8, radius * 2.1, radius * 0.95}, person.jacket, {radius = {radius, radius, 4, 4}})
        Characters.shape({x - radius * 1.1, top + radius * 0.55, radius * 2.2, 12}, person.shirt, {radius = 6})
        graphics2d.drawCircle(x, top - 12, 9, person.shirt)
    elseif person.hat == 'topHat' then
        Characters.shape({x - radius * 0.7, top - radius * 1.25, radius * 1.4, radius * 1.4}, '#FF2A2120', {radius = 4})
        Characters.shape({x - radius * 1.25, top + radius * 0.05, radius * 2.5, 10}, '#FF2A2120', {radius = 5})
        Characters.shape({x - radius * 0.7, top - radius * 0.2, radius * 1.4, 8}, person.shirt, {borderWidth = 0})
    elseif person.hat == 'bow' then
        for side = -1, 1, 2 do
            graphics2d.drawPolygon({{x + radius * 0.5, top + 4}, {x + radius * 0.5 + side * 18, top - 10}, {x + radius * 0.5 + side * 18, top + 18}}, person.jacket)
        end
        graphics2d.drawCircle(x + radius * 0.5, top + 4, 6, person.shirt)
    end
end

function Characters.drawArm(arm, person, sleeve)
    local chain = arm.chain
    local upper, lower = arm.upper, arm.lower
    upper[1], upper[2] = chain:point(1)
    upper[3], upper[4] = chain:point(2)
    lower[1], lower[2] = upper[3], upper[4]
    lower[3], lower[4] = chain:point(3)
    local width = 20 * person.build
    graphics2d.drawStroke(upper, {width = width + 6, color = Characters.outline, cap = 'round'})
    graphics2d.drawStroke(lower, {width = width + 2, color = Characters.outline, cap = 'round'})
    graphics2d.drawStroke(upper, {width = width, color = sleeve, cap = 'round'})
    graphics2d.drawStroke(lower, {width = width - 4, color = person.top == 'tee' and person.skin or sleeve, cap = 'round'})
    graphics2d.drawCircle(lower[3], lower[4], width * 0.55, Characters.outline)
    graphics2d.drawCircle(lower[3], lower[4], width * 0.55 - 3, person.skin)
end

function Characters.drawPerson(person, time)
    local feet, hip, shoulder, head = Characters.pose(person, time)
    local x, size, width = person.x, person.size, 44 * person.build
    local radius = person.head

    graphics2d.drawEllipse(x, person.ground + 4, 70 * person.build, 10, '#55000000')
    Characters.drawHairBack(person, x, head)
    for side = -1, 1, 2 do
        local legX = x + side * 17 * person.build
        local legColor = person.bottom == 'pants' and person.legs or person.skin
        Characters.shape({legX - 13 * person.build, hip - 6, 26 * person.build, feet - hip}, legColor, {radius = 10})
        if person.bottom == 'shorts' then
            Characters.shape({legX - 14 * person.build, hip - 6, 28 * person.build, 50 * size}, person.legs, {radius = 8})
        end
        Characters.shape({legX - 18 + side * 4, feet - 14, 36, 18}, person.shoes, {kind = 'ellipse'})
    end
    if person.bottom == 'skirt' or person.top == 'dress' then
        local color = person.top == 'dress' and person.shirt or person.legs
        graphics2d.drawPolygon({{x - width * 0.9, hip - 10}, {x + width * 0.9, hip - 10}, {x + width * 1.2, hip + 60 * size}, {x - width * 1.2, hip + 60 * size}}, color)
        graphics2d.drawPolyline({{x - width * 0.9, hip - 10}, {x + width * 0.9, hip - 10}, {x + width * 1.2, hip + 60 * size}, {x - width * 1.2, hip + 60 * size}}, 3, Characters.outline, true)
    end
    local body = person.top == 'jacket' and person.jacket or person.shirt
    Characters.shape({x - width, shoulder, width * 2, hip - shoulder + 10}, body, {radius = {22, 22, 10, 10}})
    if person.top == 'jacket' then
        Characters.shape({x - 12, shoulder + 4, 24, hip - shoulder}, person.shirt, {radius = 4, borderWidth = 2})
    elseif person.top == 'sweater' then
        Characters.shape({x - 18, shoulder - 4, 36, 14}, m.color(body):lerp(m.color('#FFFFFFFF'), 0.25), {radius = 7, borderWidth = 2})
    end
    Characters.shape({x - 10, shoulder - 14, 20, 20}, person.skin, {radius = 4, borderWidth = 0})
    for _, arm in ipairs(person.arms) do
        Characters.drawArm(arm, person, body)
    end
    graphics2d.drawCircle(x - radius * 0.98, head + 4, 8, Characters.outline)
    graphics2d.drawCircle(x + radius * 0.98, head + 4, 8, Characters.outline)
    graphics2d.drawCircle(x - radius * 0.98, head + 4, 5.5, person.skin)
    graphics2d.drawCircle(x + radius * 0.98, head + 4, 5.5, person.skin)
    Characters.shape({x - radius, head - radius, radius * 2, radius * 2.05}, person.skin, {kind = 'ellipse'})
    Characters.drawFace(person, x, head)
    Characters.drawHairFront(person, x, head)
    if person.hat then
        Characters.drawHat(person, x, head)
    end
    graphics2d.drawText(nil, person.name, x, person.ground + 14, {size = 22, color = '#FFE8EAF2', anchor = {0.5, 0}})
end

function Characters:draw(area)
    local time = haylen.elapsed()
    for _, ground in ipairs(Characters.grounds) do
        graphics2d.drawRect({-800, ground, 1600, 3}, '#FF2C3147')
    end
    for _, person in ipairs(self.people) do
        Characters.drawPerson(person, time)
    end
end

return Characters
