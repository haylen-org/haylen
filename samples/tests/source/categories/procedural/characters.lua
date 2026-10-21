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
local person = {size = random:range(0.9, 1.08), skin = pick(skins), hair = pick(hairStyles), head = random:range(54, 60)}  -- A head of about 40 percent.
local arm = animation2d.newChain({x = shoulderX, y = shoulderY, lengths = {34, 32}})
arm:reach(shoulderX + side * 10, shoulderY + 58, {pole = {shoulderX + side * 36, shoulderY + 30}})  -- Hangs with a soft elbow.
if near(pointer) then arm:reach(shoulderX + side * 34 + math.sin(time * 10) * 10, shoulderY - 44) end  -- Waves.]]

Characters.outline = '#FF1E1B33'
Characters.skins = {'#FFFBD9C0', '#FFF1C3A0', '#FFE0A57E', '#FFC68457', '#FF9C6440', '#FF7A4A2E', '#FF5C3622'}
Characters.hairColors = {'#FF2A2120', '#FF5A3826', '#FF8A4E2A', '#FFD9B060', '#FFB8582E', '#FF9A9AA0', '#FFE8E4DC', '#FF5B7BE0', '#FFE06AA8'}
Characters.hairWeights = {14, 14, 10, 8, 6, 4, 2, 1, 1}
Characters.cloth = {'#FFE5534B', '#FF3C9BE0', '#FF35B37E', '#FFF2B23A', '#FFA87FE0', '#FF2E3A55', '#FFEFE6D8', '#FF7A5642', '#FF2B8C8C', '#FFE07A3A'}
Characters.hairStyles = {'short', 'long', 'bun', 'spiky', 'bald', 'ponytail', 'curly'}
Characters.tops = {'tee', 'sweater', 'jacket', 'dress'}
Characters.bottoms = {'pants', 'shorts', 'skirt'}
Characters.hats = {'cap', 'beanie', 'topHat', 'bow'}
Characters.hairNames = {short = 'short', long = 'long', bun = 'a bun of', spiky = 'spiky', bald = 'almost no', ponytail = 'a ponytail of', curly = 'curly'}
Characters.hatNames = {cap = 'a cap', beanie = 'a beanie', topHat = 'a top hat', bow = 'a bow'}
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

-- Makes one person standing with their feet at `x`, `ground`, in the proportions of the art style: a head about 40 percent of the height on a short body.
function Characters.make(random, x, ground)
    local pick = function(list) return list[random:integer(1, #list)] end
    local person = {x = x, ground = ground, name = names.person(random)}
    person.size = random:range(0.9, 1.08)
    person.build = random:range(0.88, 1.15)
    person.head = random:range(54, 60) * person.size
    person.skin = pick(Characters.skins)
    person.hairColor = Characters.hairColors[random:weightedIndex(Characters.hairWeights)]
    -- Hair always reads apart from the skin under it, so a hair color near the skin turns black.
    local hair, skin = m.color(person.hairColor), m.color(person.skin)
    if math.abs(hair.r - skin.r) + math.abs(hair.g - skin.g) + math.abs(hair.b - skin.b) < 0.45 then
        person.hairColor = '#FF17121A'
    end
    person.hair = pick(Characters.hairStyles)
    person.top = pick(Characters.tops)
    person.bottom = person.top == 'dress' and 'none' or pick(Characters.bottoms)
    person.shirt, person.jacket, person.legs, person.shoes = pick(Characters.cloth), pick(Characters.cloth), pick(Characters.cloth), pick({'#FF2A2120', '#FF5A3826', '#FFEFE6D8', '#FFE5534B'})
    person.hat = random:chance(0.4) and pick(Characters.hats) or nil
    -- A bun sits on top of the head, where a hat goes, so a person with a hat wears the hair short.
    if person.hat and person.hair == 'bun' then
        person.hair = 'short'
    end
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
        person.arms[#person.arms + 1] = {side = side, chain = animation2d.newChain({lengths = {34 * person.size, 32 * person.size}, angle = math.pi / 2}), reach = {pole = {0, 0}}, upper = {0, 0, 0, 0}, lower = {0, 0, 0, 0}}
    end
    return person
end

-- Returns the place of the feet, the hips, the shoulders and the center of the head of a person this frame.
function Characters.pose(person, time)
    local size = person.size
    local breath = math.sin(time * 2 + person.phase) * 1.5
    local feet = person.ground - person.hop
    local hip = feet - 62 * size
    local shoulder = hip - 58 * size + breath
    local head = shoulder - person.head * 0.82
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
        local over = math.abs(pointer.worldX - person.x) < 100 and pointer.worldY > head - person.head and pointer.worldY < person.ground
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
        local width = 32 * person.build
        for _, arm in ipairs(person.arms) do
            local side = arm.side
            local shoulderX, shoulderY = person.x + side * (width - 9), shoulder + 14
            arm.chain.x, arm.chain.y = shoulderX, shoulderY
            local handX, handY, poleX, poleY
            local waving = near and (side > 0) == (pointer.worldX >= person.x)
            -- A hanging arm bends a little at the elbow, and a waving arm lifts the hand over the shoulder with the elbow out to the side.
            if waving then
                handX = shoulderX + side * (34 + math.sin(time * 10 + person.phase) * 10) * person.size
                handY = shoulderY - 44 * person.size
                poleX, poleY = shoulderX + side * 70, shoulderY + 24
            else
                handX = shoulderX + side * (10 + math.sin(time * 1.6 + person.phase) * 3) * person.size
                handY = shoulderY + 58 * person.size
                poleX, poleY = shoulderX + side * 36, shoulderY + 30
            end
            arm.reach.pole[1], arm.reach.pole[2] = poleX, poleY
            arm.chain:reach(handX, handY, arm.reach)
        end
    end
    if hovered then
        local wears = hovered.bottom == 'none' and 'a dress' or string.format('a %s and %s', hovered.top, hovered.bottom)
        self:status(string.format('%s has %s hair and wears %s%s%s.', hovered.name, Characters.hairNames[hovered.hair], wears, hovered.hat and (' with ' .. Characters.hatNames[hovered.hat]) or '', hovered.glasses and ' and glasses' or ''))
    else
        self:status(string.format('A crowd of %d people made from the seed %d. Point at someone to read them.', #self.people, self.seed))
    end
end

-- Draws a shape with the outline of the crowd.
function Characters.shape(rect, color, options)
    options = options or {}
    options.color, options.borderWidth, options.borderColor = color, options.borderWidth or 3.5, Characters.outline
    graphics2d.drawShape(rect, options)
end

-- A soft highlight on a part, from the top left as the light of the art style comes.
function Characters.shine(rect, radius)
    graphics2d.drawShape(rect, {radius = radius, color = '#40FFFFFF'})
end

function Characters.drawHairBack(person, x, head)
    local radius = person.head
    if person.hair == 'long' then
        Characters.shape({x - radius * 1.12, head - radius * 0.85, radius * 2.24, radius * 2.15}, person.hairColor, {radius = {radius, radius, radius * 0.3, radius * 0.3}})
    elseif person.hair == 'ponytail' then
        Characters.shape({x + radius * 0.7, head - radius * 0.35, radius * 0.7, radius * 1.4}, person.hairColor, {kind = 'ellipse', rotation = -0.35})
    elseif person.hair == 'bun' then
        Characters.shape({x - radius * 0.42, head - radius * 1.5, radius * 0.84, radius * 0.8}, person.hairColor, {kind = 'ellipse'})
    elseif person.hair == 'curly' then
        for index = 0, 8 do
            local angle = math.pi * 0.95 + index / 8 * math.pi * 1.1
            Characters.shape({x + math.cos(angle) * radius * 1.0 - radius * 0.36, head + math.sin(angle) * radius * 1.0 - radius * 0.36, radius * 0.72, radius * 0.72}, person.hairColor, {kind = 'ellipse'})
        end
    end
end

-- The hair over the forehead, a fringe that follows the top of the head, drawn after the face.
function Characters.drawHairFront(person, x, head)
    local radius = person.head
    local color = person.hairColor
    if person.hair == 'short' or person.hair == 'long' or person.hair == 'bun' or person.hair == 'ponytail' or person.hair == 'curly' then
        Characters.shape({x - radius * 1.04, head - radius * 1.06, radius * 2.08, radius * 1.3}, color, {kind = 'ellipse', startAngle = math.pi, sweep = math.pi})
        graphics2d.drawShape({x - radius * 1.0, head - radius * 0.47, radius * 2.0, radius * 0.24}, {radius = {0, 0, radius * 0.12, radius * 0.12}, color = color})
        Characters.shine({x - radius * 0.62, head - radius * 0.84, radius * 0.5, radius * 0.16}, radius * 0.08)
    elseif person.hair == 'spiky' then
        local points = {}
        for index = 0, 10 do
            local angle = math.pi + index / 10 * math.pi
            local reach = radius * (index % 2 == 0 and 1.02 or 1.32)
            points[#points + 1] = {x + math.cos(angle) * reach, head - radius * 0.1 + math.sin(angle) * reach}
        end
        points[#points + 1] = {x + radius * 1.02, head - radius * 0.35}
        points[#points + 1] = {x - radius * 1.02, head - radius * 0.35}
        local flat = {}
        for _, point in ipairs(points) do
            flat[#flat + 1] = point[1]
            flat[#flat + 1] = point[2]
        end
        graphics2d.drawStroke(flat, {width = 6, color = Characters.outline, join = 'round', closed = true})
        graphics2d.drawPolygon(points, color)
    elseif person.hair == 'bald' then
        Characters.shine({x - radius * 0.55, head - radius * 0.82, radius * 0.6, radius * 0.22}, radius * 0.11)
    end
end

function Characters.drawFace(person, x, head)
    local radius = person.head
    local eyeY = head + radius * 0.12
    local outline = Characters.outline
    for side = -1, 1, 2 do
        local eyeX = x + side * radius * 0.4
        if person.blinkIn < 0 then
            graphics2d.drawStroke({eyeX - radius * 0.13, eyeY, eyeX + radius * 0.13, eyeY}, {width = 3.5, color = outline, cap = 'round'})
        else
            graphics2d.drawShape({eyeX - radius * 0.12, eyeY - radius * 0.17, radius * 0.24, radius * 0.34}, {kind = 'ellipse', color = outline})
            graphics2d.drawCircle(eyeX + radius * 0.04, eyeY - radius * 0.07, radius * 0.06, '#FFFFFFFF')
        end
        graphics2d.drawStroke({eyeX - radius * 0.14, eyeY - radius * 0.3 - side, eyeX + radius * 0.14, eyeY - radius * 0.3 + side}, {width = 3.5, color = outline, cap = 'round'})
        if person.blush then
            graphics2d.drawEllipse(x + side * radius * 0.62, head + radius * 0.42, radius * 0.16, radius * 0.09, '#66FF6A6A')
        end
        if person.glasses then
            graphics2d.drawRing(eyeX, eyeY, radius * 0.26, 3, outline)
        end
    end
    if person.glasses then
        graphics2d.drawLine(x - radius * 0.14, eyeY, x + radius * 0.14, eyeY, 3, outline)
    end
    if person.freckles then
        for index = -2, 2 do
            graphics2d.drawCircle(x + index * radius * 0.1, head + radius * 0.36 + (index % 2) * 3, 1.5, '#AA8A4E2A')
        end
    end
    graphics2d.drawEllipse(x, head + radius * 0.34, radius * 0.07, radius * 0.05, m.color(person.skin):lerp(m.color(outline), 0.25))
    -- A beard runs from one ear to the other along the jaw, joined to the hair at the sideburns.
    if person.beard then
        Characters.shape({x - radius * 0.96, head + radius * 0.05, radius * 1.92, radius * 1.02}, person.hairColor, {kind = 'ellipse', startAngle = 0, sweep = math.pi})
        graphics2d.drawShape({x - radius * 0.3, head + radius * 0.44, radius * 0.6, radius * 0.24}, {kind = 'ellipse', color = person.skin})
    end
    local mouthY = head + radius * 0.56
    if person.smile == 'smile' then
        graphics2d.drawArc(x, mouthY - radius * 0.1, radius * 0.17, 3.5, 0.45, math.pi - 0.45, outline)
    elseif person.smile == 'grin' then
        Characters.shape({x - radius * 0.2, mouthY - radius * 0.08, radius * 0.4, radius * 0.24}, '#FFFFFFFF', {kind = 'ellipse', startAngle = 0, sweep = math.pi, borderWidth = 3})
    elseif person.smile == 'open' then
        Characters.shape({x - radius * 0.09, mouthY - radius * 0.08, radius * 0.18, radius * 0.2}, '#FF8A2A2E', {kind = 'ellipse', borderWidth = 3})
    else
        graphics2d.drawStroke({x - radius * 0.12, mouthY, x + radius * 0.12, mouthY}, {width = 3.5, color = outline, cap = 'round'})
    end
end

-- Hats seen from the front: a cap with its peak toward the viewer, a beanie with a pompom, a top hat and a bow on the side of the head.
function Characters.drawHat(person, x, head)
    local radius = person.head
    local top = head - radius
    if person.hat == 'cap' then
        Characters.shape({x - radius * 1.06, top - radius * 0.12, radius * 2.12, radius * 1.15}, person.jacket, {kind = 'ellipse', startAngle = math.pi, sweep = math.pi})
        Characters.shine({x - radius * 0.6, top + radius * 0.04, radius * 0.5, radius * 0.18}, radius * 0.09)
        Characters.shape({x - radius * 0.86, top + radius * 0.36, radius * 1.72, radius * 0.3}, m.color(person.jacket):lerp(m.color('#FF000000'), 0.25), {kind = 'ellipse'})
        graphics2d.drawCircle(x, top - radius * 0.1, radius * 0.08, person.shirt)
    elseif person.hat == 'beanie' then
        Characters.shape({x - radius * 1.04, top - radius * 0.2, radius * 2.08, radius * 0.95}, person.jacket, {radius = {radius, radius, 6, 6}})
        Characters.shape({x - radius * 1.1, top + radius * 0.5, radius * 2.2, radius * 0.26}, person.shirt, {radius = radius * 0.13})
        Characters.shape({x - radius * 0.2, top - radius * 0.42, radius * 0.4, radius * 0.4}, person.shirt, {kind = 'ellipse'})
    elseif person.hat == 'topHat' then
        Characters.shape({x - radius * 0.66, top - radius * 1.0, radius * 1.32, radius * 1.22}, '#FF2A2120', {radius = 6})
        Characters.shape({x - radius * 0.66, top - radius * 0.1, radius * 1.32, radius * 0.2}, person.shirt, {borderWidth = 0})
        Characters.shape({x - radius * 1.2, top + radius * 0.14, radius * 2.4, radius * 0.22}, '#FF2A2120', {radius = radius * 0.11})
    elseif person.hat == 'bow' then
        local bowX = x + radius * 0.55
        for side = -1, 1, 2 do
            Characters.shape({bowX + (side > 0 and 0 or -radius * 0.44), top - radius * 0.02, radius * 0.44, radius * 0.4}, person.jacket, {kind = 'ellipse', rotation = side * 0.3})
        end
        Characters.shape({bowX - radius * 0.12, top + radius * 0.06, radius * 0.24, radius * 0.24}, person.shirt, {kind = 'ellipse'})
    end
end

-- An arm is one rounded stroke from the shoulder through the elbow to the hand, outlined as one shape, in the sleeve down to the elbow or the wrist and the skin below it.
function Characters.drawArm(arm, person, sleeve)
    local chain = arm.chain
    local upper, lower = arm.upper, arm.lower
    upper[1], upper[2] = chain:point(1)
    upper[3], upper[4] = chain:point(2)
    lower[1], lower[2] = upper[3], upper[4]
    lower[3], lower[4] = chain:point(3)
    local width = 17 * person.build
    graphics2d.drawStroke(upper, {width = width + 6, color = Characters.outline, cap = 'round'})
    graphics2d.drawStroke(lower, {width = width + 6, color = Characters.outline, cap = 'round'})
    graphics2d.drawStroke(lower, {width = width, color = person.top == 'tee' and person.skin or sleeve, cap = 'round'})
    graphics2d.drawStroke(upper, {width = width, color = sleeve, cap = 'round'})
    graphics2d.drawCircle(lower[3], lower[4], width * 0.62 + 3, Characters.outline)
    graphics2d.drawCircle(lower[3], lower[4], width * 0.62, person.skin)
end

function Characters.drawPerson(person, time)
    local feet, hip, shoulder, head = Characters.pose(person, time)
    local x, size, width = person.x, person.size, 32 * person.build
    local radius = person.head

    graphics2d.drawShape({x - 52 * person.build, person.ground - 6, 104 * person.build, 16}, {kind = 'ellipse', color = '#55000000', softness = 6})
    Characters.drawHairBack(person, x, head)
    for side = -1, 1, 2 do
        local legX = x + side * 13 * person.build
        local legColor = person.bottom == 'pants' and person.legs or person.skin
        Characters.shape({legX - 11 * person.build, hip - 8, 22 * person.build, feet - hip + 2}, legColor, {radius = 9})
        if person.bottom == 'shorts' then
            Characters.shape({legX - 12.5 * person.build, hip - 8, 25 * person.build, 30 * size}, person.legs, {radius = 8})
        end
        Characters.shape({legX - 15 + side * 4, feet - 13, 30, 17}, person.shoes, {radius = {10, 10, 6, 6}})
    end
    if person.bottom == 'skirt' or person.top == 'dress' then
        local color = person.top == 'dress' and person.shirt or person.legs
        Characters.shape({x - width * 1.12, hip - 14, width * 2.24, 40 * size}, color, {radius = {8, 8, 18, 18}})
    end
    -- The torso has round shoulders, wide as the build, and the arms come from inside its top corners.
    local body = person.top == 'jacket' and person.jacket or person.shirt
    Characters.shape({x - width, shoulder - 4, width * 2, hip - shoulder + 12}, body, {radius = {width * 0.75, width * 0.75, 12, 12}})
    Characters.shine({x - width * 0.6, shoulder + 2, width * 0.55, 10}, 5)
    if person.top == 'jacket' then
        Characters.shape({x - 9, shoulder + 2, 18, hip - shoulder + 2}, person.shirt, {radius = 4, borderWidth = 2.5})
    elseif person.top == 'sweater' then
        Characters.shape({x - 15, shoulder - 6, 30, 12}, m.color(body):lerp(m.color('#FFFFFFFF'), 0.25), {radius = 6, borderWidth = 2.5})
    end
    for _, arm in ipairs(person.arms) do
        Characters.drawArm(arm, person, body)
    end
    for side = -1, 1, 2 do
        Characters.shape({x + side * radius * 0.94 - radius * 0.13, head + radius * 0.02, radius * 0.26, radius * 0.32}, person.skin, {kind = 'ellipse'})
    end
    Characters.shape({x - radius, head - radius, radius * 2, radius * 1.96}, person.skin, {kind = 'ellipse', borderWidth = 3.5})
    Characters.drawFace(person, x, head)
    Characters.drawHairFront(person, x, head)
    if person.hat then
        Characters.drawHat(person, x, head)
    end
    graphics2d.drawText(nil, person.name, x, person.ground + 16, {size = 24, color = '#FFE8EAF2', anchor = {0.5, 0}})
end

-- Every row stands on a strip of grass that fills the width of the canvas.
function Characters:draw(area)
    local time = haylen.elapsed()
    local bounds = graphics2d.canvasBounds()
    for _, ground in ipairs(Characters.grounds) do
        graphics2d.drawShape({bounds.x, ground - 4, bounds.width, 12}, {radius = 6, color = '#FF3D5A3A'})
        graphics2d.drawRect({bounds.x, ground - 4, bounds.width, 3}, '#FF6FA854')
    end
    for _, person in ipairs(self.people) do
        Characters.drawPerson(person, time)
    end
end

return Characters
