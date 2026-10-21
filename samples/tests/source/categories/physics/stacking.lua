-- Stacking blocks of many shapes from a crane trolley into the highest tower that stands, measured from the bounds of the settled blocks, with the collapses it detects. The trolley rides above the top of the tower, and the camera rises with it. The first blocks drop by themselves when the test opens.
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local input = require('haylen.input')
local m = require('haylen.math')
local physics2d = require('haylen.physics2d')
local ui = require('haylen.ui')

local Look = require('categories.physics.look')
local PhysicsTest = require('categories.physics.physics-test')
local Scenery = require('categories.physics.scenery')

local Stacking = haylen.class('Stacking', PhysicsTest)

Stacking.code = [[
local block = world:createBody({type = 'kinematic', x = trolleyX, y = magnetY + 15})
shape.mask = 0  -- Hangs under the magnet and collides with nothing.
block:setTransform(trolleyX, magnetY + 15, 0)
block.type = 'dynamic'  shape.mask = -1  -- Drops it.
local top = shape.bounds.y  -- The top of a settled block measures the tower.]]

Stacking.actions = {
    {name = 'drop', type = 'button', bindings = {'key:space', 'button:south', 'virtual:drop'}},
}

Stacking.ground = 390
Stacking.baseTop = 315
-- How far above the best tower the magnet rides and how high the camera keeps it in the view.
Stacking.clearance = 230
Stacking.cable = 70
Stacking.swing = 320
Stacking.dropDelay = 0.5
Stacking.maxBlocks = 40
Stacking.settled = 25
Stacking.collapseTime = 0.7
Stacking.firstDrops = 4
Stacking.material = {friction = 0.8, density = 1}
-- The blocks, each with the top of its outline under the magnet and how far its body sits beside the magnet.
Stacking.kinds = {
    {material = 'wood', top = 15, box = {120, 30}},
    {material = 'woodCrate', top = 30, box = {60, 60}},
    {material = 'stone', top = 45, box = {30, 90}},
    {material = 'red', top = 25, polygon = {{-50, 25}, {50, 25}, {25, -25}, {-25, -25}}},
    {material = 'yellow', top = 28, circle = 28},
    {material = 'blue', top = 45, offset = 30, polygon = {{-45, -45}, {-15, -45}, {-15, 15}, {45, 15}, {45, 45}, {-45, 45}}},
    {material = 'green', top = 16, capsule = {-40, 0, 40, 0, 16}},
}

function Stacking:enter()
    self:frame{
        code = Stacking.code,
        hint = 'Drop the block that hangs from the moving trolley with a click, a tap, Space or the south button, and build the highest tower on the stand. R or the X button clears the stand.',
        controls = {
            ui.button{id = 'drop', text = 'Drop the block', onClick = function() self:release() end},
            ui.label{text = 'Speed of the trolley', font = 'caption', color = 'textMuted'},
            ui.slider{id = 'speed', value = 1, min = 0, max = 2.5, step = 0.1, showValue = true, decimals = 1, onChange = function(event) self.speed = event.value end},
            ui.button{id = 'reset', text = 'Clear the stand', onClick = function() self:build() end},
        },
        actions = Stacking.actions,
        focus = 'drop',
        view = {1200, 530},
    }
    self.random = m.random(47)
    self.speed = 1
    self:build()
    self.autoDrops = Stacking.firstDrops
end

function Stacking:build()
    self.world = physics2d.newWorld()
    local ground = self.world:createBody({type = 'static'})
    ground:addBox(4000, 40, {offsetY = Stacking.ground + 20})
    ground:addBox(320, 30, {offsetY = Stacking.baseTop + 15, friction = 0.9})
    ground:addBox(60, 45, {offsetY = Stacking.baseTop + 52})
    self.blocks = {}
    self.phase, self.cooldown, self.best, self.height, self.collapses, self.dropped, self.falling = 0, 0, 0, 0, 0, 0, 0
    self.lift, self.autoDrops = 0, 0
    self.message, self.messageTime = nil, 0
    self:hang()

    -- A block counts toward the height once it touched the stand, the ground or another block, so a falling block never does.
    self.world.onContactBegin = function(a, b, contact)
        if a.data and a.data.kind then
            a.data.landed = true
        end
        if b.data and b.data.kind then
            b.data.landed = true
        end
    end
end

function Stacking:trolleyX()
    return math.sin(self.phase * 1.1) * Stacking.swing
end

-- The magnet hangs its cable below the trolley, which rides the clearance above the best tower.
function Stacking:magnetY()
    return Stacking.baseTop - Stacking.clearance - self.lift + Stacking.cable
end

-- Hangs the next block under the magnet, where it is now, as a kinematic body whose shapes collide with nothing until it drops.
function Stacking:hang()
    local kind = Stacking.kinds[self.random:integer(1, #Stacking.kinds)]
    local body = self.world:createBody({type = 'kinematic', x = self:trolleyX() + (kind.offset or 0), y = self:magnetY() + kind.top})
    if kind.box then
        body:addBox(kind.box[1], kind.box[2], Stacking.material)
    elseif kind.circle then
        body:addCircle(kind.circle, Stacking.material)
    elseif kind.capsule then
        local capsule = kind.capsule
        body:addCapsule(capsule[1], capsule[2], capsule[3], capsule[4], capsule[5], Stacking.material)
    else
        body:addPolygon(kind.polygon, Stacking.material)
    end
    Look.dress(body, kind.material, {outline = kind.polygon})
    body.data.kind = kind
    body.data.shapes = body:shapes()
    for _, shape in ipairs(body.data.shapes) do
        shape.mask = 0
    end
    self.held = body
end

function Stacking:release()
    if self.cooldown > 0 then
        return
    end
    local body = self.held
    for _, shape in ipairs(body.data.shapes) do
        shape.mask = -1
    end
    body.type = 'dynamic'
    body.velocity = {0, 0}
    self.blocks[#self.blocks + 1] = body
    if #self.blocks > Stacking.maxBlocks then
        table.remove(self.blocks, 1):destroy()
    end
    self.cooldown = Stacking.dropDelay
    self:hang()
end

-- The tower is as high as the top of the blocks that landed on the stand and rest there, and a tower that stays under most of its best height for a moment has collapsed, while a block that only bounces has not.
function Stacking:measure(dt)
    local top = Stacking.baseTop
    for _, body in ipairs(self.blocks) do
        if body.data.landed and body.velocity:length() < Stacking.settled and math.abs(body.x) < 220 and body.y < Stacking.baseTop then
            for _, shape in ipairs(body.data.shapes) do
                top = math.min(top, shape.bounds.y)
            end
        end
    end
    self.height = Stacking.baseTop - top
    if self.best > 150 and self.height < self.best * 0.6 then
        self.falling = self.falling + dt
        if self.falling > Stacking.collapseTime then
            self.collapses = self.collapses + 1
            self.message, self.messageTime = string.format('The tower of %d units collapsed', math.floor(self.best + 0.5)), 2
            self.best, self.falling = self.height, 0
        end
    else
        self.falling = 0
    end
    self.best = math.max(self.best, self.height)
end

function Stacking:update(dt)
    Stacking.super.update(self, dt)
    if input.pressed('reset') then
        self:build()
    end
    if input.pressed('drop') or self.pointer.pressed and not ui.usingPointer() then
        self.autoDrops = 0
        self:release()
    end
    self.messageTime = math.max(0, self.messageTime - dt)
    self:measure(dt)

    -- The trolley and the camera rise smoothly with the best tower and come down after a collapse.
    self.lift = self.lift + (self.best - self.lift) * math.min(1, dt * 2)
    self.camera.position = {0, 190 - self.lift}

    local blocks = #self.blocks
    local collapses = self.collapses == 0 and 'has not collapsed yet' or self.collapses == 1 and 'collapsed once' or string.format('collapsed %d times', self.collapses)
    self:status(string.format('The tower stands %d units high and the best one reached %d. %d of %d blocks missed the stand, and the tower %s.', math.floor(self.height + 0.5), math.floor(self.best + 0.5), self.dropped, blocks, collapses))
end

function Stacking:fixedUpdate(step)
    self.phase = self.phase + step * self.speed
    self.cooldown = math.max(0, self.cooldown - step)
    local kind = self.held.data.kind
    self.held:setTransform(self:trolleyX() + (kind.offset or 0), self:magnetY() + kind.top, 0)

    -- The first blocks drop by themselves while the trolley passes over the stand.
    if self.autoDrops > 0 and self.cooldown == 0 and math.abs(self:trolleyX()) < 40 then
        self.autoDrops = self.autoDrops - 1
        self:release()
        self.cooldown = 2
    end
    self:simulate(self.world, step)

    -- A block that missed the stand stays where it settled on the ground and counts once as dropped off, until the oldest blocks make room.
    for _, body in ipairs(self.blocks) do
        if not body.data.off and body.y > Stacking.baseTop + 30 and body.velocity:length() < Stacking.settled then
            body.data.off = true
            self.dropped = self.dropped + 1
        end
    end
end

-- The crane is a steel beam across the view with a trolley that runs on it, a cable and an electromagnet that holds the next block by its top.
function Stacking:drawCrane()
    local bounds = graphics2d.canvasBounds()
    local x = self:trolleyX()
    local magnet = self:magnetY()
    local beam = magnet - Stacking.cable - 22
    Look.block(bounds.x + bounds.width / 2, beam, bounds.width + 40, 26, 'iron', 2)
    Look.block(x, beam + 22, 70, 26, 'yellow', 3)
    Look.disc(x - 22, beam + 8, 9, 'steel', 3)
    Look.disc(x + 22, beam + 8, 9, 'steel', 3)
    Look.line(x, beam + 35, x, magnet - 14, 5, '#FF474F66', 3)
    Look.block(x, magnet - 8, 56, 16, 'iron', 3)
    graphics2d.drawRect({x - 26, magnet - 3, 52, 3}, '#FFF0473E', {layer = 3})
end

-- A mark is a line at a height over the stand with its value on a plate, the height on the left and the best on the right, so the two never cover each other.
function Stacking:drawMark(value, color, text, x, anchor, dashed)
    if value < 1 then
        return
    end
    local y = Stacking.baseTop - value
    if dashed then
        Look.dashed(-240, y, 240, y, color, 4)
    else
        graphics2d.drawLine(-240, y, 240, y, 3 * Look.pixel, color, {layer = 4})
    end
    self:label(string.format('%s %d', text, math.floor(value + 0.5)), x, y, {anchor = anchor, size = 20, color = color})
end

function Stacking:draw(area)
    Scenery.sky(self.camera, Stacking.ground)
    Scenery.ground(Stacking.ground)
    Look.block(0, Stacking.baseTop + 52, 60, 45, 'stone', 0)
    Look.block(0, Stacking.baseTop + 15, 320, 30, 'stone', 0)
    Look.drawAll(self.blocks, {layer = 1})
    self:drawCrane()
    Look.draw(self.held, {layer = 2})
    self:drawMark(self.height, '#FF7BC74D', 'Height', -250, {1, 0.5}, false)
    self:drawMark(self.best, '#FFFFC93C', 'Best', 250, {0, 0.5}, true)
    if self.messageTime > 0 then
        self:label(self.message, 0, Stacking.baseTop - Stacking.clearance - self.lift - 90, {size = 28, plate = PhysicsTest.problemPlate, layer = 50})
    end
end

return Stacking
