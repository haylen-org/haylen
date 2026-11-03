-- Explosive barrels in a wooden warehouse that blow up with `physics2d.explode` when they are clicked or hit hard enough, lighting the fuses of the barrels their blast reaches in a chain reaction that scatters the crates. The warehouse has three shelves under a roof beam with two lamps and a warning sign, and the first barrel goes off by itself when the test opens.
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local input = require('haylen.input')
local m = require('haylen.math')
local physics2d = require('haylen.physics2d')
local timer = require('haylen.timer')
local ui = require('haylen.ui')

local Burst = require('categories.physics.burst')
local Look = require('categories.physics.look')
local PhysicsTest = require('categories.physics.physics-test')
local Scenery = require('categories.physics.scenery')

local Barrels = haylen.class('Barrels', PhysicsTest)

Barrels.code = [[
local world = physics2d.newWorld({hitThreshold = 600})  -- Only hard hits call onHit.
local hits = physics2d.explode(world, {x = barrel.x, y = barrel.y, radius = 300, impulse = 700, falloff = 'linear'})
for _, hit in ipairs(hits) do
    light(hit.body)  -- A barrel the blast reaches goes off a moment later.
end
world.onHit = light  -- So does a barrel that a hard hit reaches.]]

Barrels.radius = 300
Barrels.hitSpeed = 600
Barrels.blastLife = 0.6
Barrels.floor = 390
Barrels.shelves = {{-300, 160}, {350, 40}, {-400, -20}}
Barrels.shelfWidth = 400
-- The roof beam runs along the top of the wall, the lamps hang from it by their cords, and the warning sign hangs between them.
Barrels.beam = -160
Barrels.lamps = {{-90, -100}, {560, -110}}
Barrels.sign = {80, -70, 46}
Barrels.firstBlast = 1.2

function Barrels:enter()
    self:frame{
        code = Barrels.code,
        hint = 'Tap or click a barrel to set it off. A blast lights the barrels it reaches, and a barrel hit hard enough by flying debris goes off too. R or the X button rebuilds.',
        controls = {
            ui.button{id = 'fire', text = 'Set off a barrel', onClick = function() self:lightFirst() end},
            ui.checkbox{id = 'chain', text = 'Chain reactions', checked = true, onChange = function(event) self.chaining = event.checked end},
            ui.label{text = 'Blast impulse', font = 'caption', color = 'textMuted'},
            ui.slider{id = 'impulse', value = 700, min = 200, max = 1600, step = 50, showValue = true, decimals = 0, onChange = function(event) self.impulse = event.value end},
            ui.button{id = 'reset', text = 'Rebuild', onClick = function() self:build() end},
        },
        focus = 'fire',
        view = {1440, 640},
    }
    self.camera.position = {0, 140}
    self.random = m.random(55)
    self.chaining, self.impulse = true, 700
    self.booms = Burst('explosions/explosion-large', {count = 6, scale = 0.7, layer = 30})
    self.glow = Scenery.glow()
    self:build()
    timer.after(Barrels.firstBlast, function() self:lightFirst() end, {owner = self})
end

function Barrels:build()
    if self.world then
        self.world:destroy()
    end
    self.world = physics2d.newWorld({owner = self, hitThreshold = Barrels.hitSpeed})
    self.barrels, self.crates = {}, {}
    self.exploded, self.longest, self.depth = 0, 0, 0

    local ground = self.world:createBody({type = 'static'})
    ground:addBox(4000, 40, {offsetY = Barrels.floor + 20})
    for _, shelf in ipairs(Barrels.shelves) do
        ground:addBox(Barrels.shelfWidth, 20, {offsetX = shelf[1], offsetY = shelf[2]})
    end
    ground:addBox(40, 1200, {offsetX = -760})
    ground:addBox(40, 1200, {offsetX = 760})

    for _, x in ipairs({-600, -300, 0, 300, 600}) do
        self:barrel(x, 355)
    end
    self:barrel(-420, 115)
    self:barrel(-180, 115)
    self:barrel(350, -5)
    self:barrel(-300, -65)
    for _, stack in ipairs({{-450, 390, 3}, {-150, 390, 2}, {150, 390, 3}, {450, 390, 2}, {-300, 150, 2}, {250, 30, 2}, {450, 30, 1}, {-500, -30, 2}, {-400, -30, 1}}) do
        for level = 1, stack[3] do
            local crate = self.world:createBody({x = stack[1], y = stack[2] - 30 - (level - 1) * 61})
            crate:addBox(60, 60)
            Look.dress(crate, 'woodCrate')
            self.crates[#self.crates + 1] = crate
        end
    end

    self.world.onHit = function(a, b)
        self:hit(a)
        self:hit(b)
    end
end

function Barrels:barrel(x, y)
    local barrel = self.world:createBody({x = x, y = y})
    barrel:addBox(50, 70, {density = 1.5})
    Look.dress(barrel, 'barrel')
    barrel.data.barrel = true
    self.barrels[#self.barrels + 1] = barrel
end

function Barrels:hit(body)
    if body and body.valid and body.data and body.data.barrel then
        self:light(body, self.depth + 1, 0.05)
    end
end

function Barrels:light(barrel, depth, fuse)
    if not barrel.data.fuse then
        barrel.data.fuse, barrel.data.depth = fuse, depth
    end
end

function Barrels:lightFirst()
    if #self.barrels > 0 then
        self:light(self.barrels[1], 1, 0)
    end
end

-- The barrel goes before its blast, so the blast only pushes what is around it, and every barrel the blast reaches gets a short fuse.
function Barrels:explode(index)
    local barrel = table.remove(self.barrels, index)
    local x, y, depth = barrel.x, barrel.y, barrel.data.depth
    barrel:destroy()
    local hits = physics2d.explode(self.world, {x = x, y = y, radius = Barrels.radius, impulse = self.impulse, falloff = 'linear'})
    self.booms:fire(x, y)
    self.camera:addTrauma(0.35)
    self.exploded, self.depth = self.exploded + 1, depth
    self.longest = math.max(self.longest, depth)
    if self.chaining then
        for _, hit in ipairs(hits) do
            if hit.body.data and hit.body.data.barrel then
                self:light(hit.body, depth + 1, self.random:range(0.12, 0.25))
            end
        end
    end
end

function Barrels:update(dt)
    Barrels.super.update(self, dt)
    if input.pressed('reset') then
        self:build()
    end
    self.camera:update(dt)
    self.booms:update(dt)
    if self.pointer.pressed and not ui.usingPointer() then
        for _, shape in ipairs(self.world:pick(self.camera, self.pointer.x, self.pointer.y, {radius = 12})) do
            if shape.body.data and shape.body.data.barrel then
                self:light(shape.body, 1, 0)
                break
            end
        end
    end
    local chain = self.longest == 0 and 'no chain has run yet' or string.format('the longest chain set off %d %s in a row', self.longest, self.longest == 1 and 'barrel' or 'barrels')
    local waiting = #self.barrels
    self:status(string.format('%d %s, %d went off, and %s.', waiting, waiting == 1 and 'barrel waits' or 'barrels wait', self.exploded, chain))
end

function Barrels:fixedUpdate(step)
    for index = #self.barrels, 1, -1 do
        local data = self.barrels[index].data
        if data.fuse then
            data.fuse = data.fuse - step
            if data.fuse <= 0 then
                self:explode(index)
            end
        end
    end
    self:simulate(self.world, step)
end

-- A lamp hangs from the roof beam by a cord: a green metal shade over a warm bulb that lights the wall around it.
function Barrels:drawLamp(x, y)
    local unit = Look.pixel
    Look.line(x, Barrels.beam + 15, x, y - 24, 3, '#FF2F3352', -26)
    graphics2d.draw(self.glow, x, y + 20, {width = 340, height = 300, color = '#59FFC970', blend = 'additive', layer = -25})
    graphics2d.drawShape({x - 34, y - 26, 68, 52}, {kind = 'ellipse', startAngle = math.pi, sweep = math.pi, color = '#FF3E7A5A', borderWidth = Look.outlinePixels * unit, borderColor = Look.ink, layer = -24})
    graphics2d.drawCircle(x, y + 2, 11, '#FFFFF2B0', {layer = -24})
    graphics2d.drawShape({x - 20, y - 18, 16, 8}, {kind = 'ellipse', rotation = -0.4, color = '#995FA884', layer = -24})
end

-- The warning sign is a yellow diamond with a black border and a flame, which hangs from two nails.
function Barrels:drawSign()
    local unit = Look.pixel
    local x, y, size = Barrels.sign[1], Barrels.sign[2], Barrels.sign[3]
    graphics2d.drawShape({x - size, y - size, size * 2, size * 2}, {radius = 10, rotation = math.pi / 4, color = '#FFFFC93C', borderWidth = Look.outlinePixels * unit, borderColor = Look.ink, layer = -24})
    graphics2d.drawShape({x - size + 9, y - size + 9, size * 2 - 18, size * 2 - 18}, {radius = 6, rotation = math.pi / 4, color = '#00000000', borderWidth = 4 * unit, borderColor = '#FF2F2B45', layer = -24})
    graphics2d.drawShape({x - 16, y - 22, 32, 44}, {kind = 'ellipse', color = '#FFF0473E', layer = -24})
    graphics2d.drawShape({x - 13, y - 34, 18, 30}, {kind = 'ellipse', rotation = 0.35, color = '#FFF0473E', layer = -24})
    graphics2d.drawShape({x - 9, y - 6, 18, 24}, {kind = 'ellipse', color = '#FFFF9A3C', layer = -24})
    graphics2d.drawShape({x - 5, y + 4, 10, 12}, {kind = 'ellipse', color = '#FFFFF0A0', layer = -24})
end

-- The warehouse is a wall of dark planks between two posts under a roof beam, with a plank floor, wooden shelves on iron brackets, lamps and a warning sign.
function Barrels:drawWarehouse()
    local unit = Look.pixel
    local bounds = graphics2d.canvasBounds()
    graphics2d.drawRect(bounds, '#FF6B4426', {layer = -30})
    local first = math.floor(bounds.x / 70) * 70
    for x = first, bounds.x + bounds.width, 70 do
        graphics2d.drawLine(x, bounds.y, x, Barrels.floor, 4 * unit, '#FF4E2F18', {layer = -29})
    end
    for _, x in ipairs({-760, 760}) do
        Look.block(x, (bounds.y + Barrels.floor) / 2, 40, Barrels.floor - bounds.y + 40, 'darkWood', -28)
    end
    Look.block(0, Barrels.beam, bounds.width + 200, 30, 'darkWood', -27)
    for _, lamp in ipairs(Barrels.lamps) do
        self:drawLamp(lamp[1], lamp[2])
    end
    self:drawSign()
    Look.block(0, Barrels.floor + 30, bounds.width + 200, 60, 'wood', -28)
    for _, shelf in ipairs(Barrels.shelves) do
        Look.block(shelf[1], shelf[2], Barrels.shelfWidth, 20, 'wood', 0)
        for _, side in ipairs({-1, 1}) do
            local x = shelf[1] + side * (Barrels.shelfWidth / 2 - 40)
            Look.line(x, shelf[2] + 10, x, shelf[2] + 50, 8, '#FF66708A', -1)
            Look.line(x, shelf[2] + 50, x - side * 30, shelf[2] + 10, 6, '#FF66708A', -1)
        end
    end
end

function Barrels:draw(area)
    self:drawWarehouse()
    Look.drawAll(self.crates, {layer = 1})
    local time = haylen.elapsed()
    for _, barrel in ipairs(self.barrels) do
        if barrel.data.fuse then
            local pulse = 0.6 + 0.4 * math.sin(time * 40)
            graphics2d.draw(self.glow, barrel.x, barrel.y, {width = 170, height = 170, color = {1, 0.75, 0.25, pulse}, blend = 'additive', layer = 0})
        end
    end
    Look.drawAll(self.barrels, {layer = 1})
    self.booms:draw()
end

return Barrels
