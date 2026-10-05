-- Stacking blocks of many shapes from a moving dropper into the highest tower that stands, measured from the bounds of the settled blocks, with the collapses it detects.
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local input = require('haylen.input')
local m = require('haylen.math')
local physics2d = require('haylen.physics2d')
local ui = require('haylen.ui')

local parts = require('categories.physics.parts')
local PhysicsTest = require('categories.physics.physics-test')

local Stacking = haylen.class('Stacking', PhysicsTest)

Stacking.actions = {
    {name = 'drop', type = 'button', bindings = {'key:space', 'button:south', 'virtual:drop'}},
}

local kBaseTop = 315
local kDropperY = -380
local kSwing = 320
local kCooldown = 0.5
local kMaxBlocks = 40
local kSettled = 25
local kCollapseTime = 0.7
local kMaterial = {friction = 0.8, density = 1}
local kKinds = {
    function(body) return {parts.box(body, 120, 30, kMaterial)} end,
    function(body) return {parts.box(body, 60, 60, kMaterial)} end,
    function(body) return {parts.box(body, 30, 90, kMaterial)} end,
    function(body) return parts.polygon(body, {{-45, 30}, {45, 30}, {0, -40}}, kMaterial) end,
    function(body) return {parts.circle(body, 28, kMaterial)} end,
    function(body) return parts.polygon(body, {{-45, -45}, {-15, -45}, {-15, 15}, {45, 15}, {45, 45}, {-45, 45}}, kMaterial) end,
    function(body) return {parts.capsule(body, -40, 0, 40, 0, 16, kMaterial)} end,
}

function Stacking:enter()
    self:frame{
        hint = 'Drop the block that hangs from the moving dropper with a click, a tap, Space or the south button, and build the highest tower on the stand. R or the X button clears the stand.',
        controls = {
            ui.button{id = 'drop', text = 'Drop the block', onClick = function() self:drop() end},
            ui.label{text = 'Speed of the dropper', font = 'caption', color = 'textMuted'},
            ui.slider{id = 'speed', value = 1, min = 0, max = 2.5, step = 0.1, showValue = true, decimals = 1, onChange = function(event) self.speed = event.value end},
            ui.button{id = 'reset', text = 'Clear the stand', onClick = function() self:build() end},
        },
        actions = Stacking.actions,
        focus = 'drop',
    }
    self.random = m.random(47)
    self.speed = 1
    self:build()
end

function Stacking:build()
    self.world = physics2d.newWorld()
    local ground = self.world:createBody({type = 'static'})
    parts.box(ground, 1600, 40, {offsetY = 410})
    parts.box(ground, 320, 30, {offsetY = kBaseTop + 15, friction = 0.9})
    parts.box(ground, 60, 45, {offsetY = 367})
    self.ground = ground
    self.blocks = {}
    self.phase, self.cooldown, self.best, self.height, self.collapses, self.dropped, self.falling = 0, 0, 0, 0, 0, 0, 0
    self.message, self.messageTime = nil, 0
    self:hang()

    -- A block counts toward the height once it touched the stand, the ground or another block, so a falling block never does.
    self.world.onContactBegin = function(a, b, contact)
        for _, body in ipairs({a, b}) do
            if body.data.shapes then
                body.data.landed = true
            end
        end
    end
end

-- Hangs the next block under the dropper as a kinematic body whose shapes collide with nothing until it drops.
function Stacking:hang()
    local body = self.world:createBody({type = 'kinematic', x = 0, y = kDropperY + 70})
    local shapes = kKinds[self.random:integer(1, #kKinds)](body)
    body.data.shapes = shapes
    for _, shape in ipairs(shapes) do
        shape.mask = 0
    end
    self.held = body
end

function Stacking:drop()
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
    if #self.blocks > kMaxBlocks then
        table.remove(self.blocks, 1):destroy()
    end
    self.cooldown = kCooldown
    self:hang()
end

-- The tower is as high as the top of the blocks that landed on the stand and rest there, and a tower that stays under most of its best height for a moment has collapsed, while a block that only bounces has not.
function Stacking:measure(dt)
    local top = kBaseTop
    for _, body in ipairs(self.blocks) do
        if body.data.landed and body.velocity:length() < kSettled and math.abs(body.x) < 220 and body.y < kBaseTop then
            for _, shape in ipairs(body.data.shapes) do
                top = math.min(top, shape.bounds.y)
            end
        end
    end
    self.height = kBaseTop - top
    if self.best > 150 and self.height < self.best * 0.6 then
        self.falling = self.falling + dt
        if self.falling > kCollapseTime then
            self.collapses = self.collapses + 1
            self.message, self.messageTime = string.format('Collapse from %.0f units', self.best), 2
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
        self:drop()
    end
    self.messageTime = math.max(0, self.messageTime - dt)
    self:measure(dt)
    self:status(string.format('Height %.0f units, best %.0f, blocks %d, dropped off %d, collapses %d, step %.2f ms', self.height, self.best, #self.blocks, self.dropped, self.collapses, self:stepTime()))
end

function Stacking:fixedUpdate(step)
    self.phase = self.phase + step * self.speed
    self.cooldown = math.max(0, self.cooldown - step)
    self.held:setTransform(math.sin(self.phase * 1.1) * kSwing, kDropperY + 70, 0)
    self:simulate(self.world, step)
    for index = #self.blocks, 1, -1 do
        local body = self.blocks[index]
        if body.y > kBaseTop + 30 and body.velocity:length() < kSettled then
            table.remove(self.blocks, index):destroy()
            self.dropped = self.dropped + 1
        end
    end
end

function Stacking:draw(area)
    parts.draw(self.ground)
    parts.drawAll(self.blocks, {layer = 1})
    local x = self.held.x
    graphics2d.drawLine(-kSwing - 80, kDropperY, kSwing + 80, kDropperY, 6, '#FF607D8B')
    graphics2d.drawRect({x - 40, kDropperY - 14, 80, 28}, '#FFFFB74D', {layer = 2})
    graphics2d.drawLine(x, kDropperY + 14, x, kDropperY + 30, 4, '#FFB0BEC5', {layer = 2})
    parts.draw(self.held, {layer = 2})
    for _, mark in ipairs({{self.height, '#FF6FDCA0', 'Height'}, {self.best, '#FFFFD54F', 'Best'}}) do
        local y = kBaseTop - mark[1]
        graphics2d.drawLine(-260, y, 260, y, 2, mark[2], {layer = 3})
        Stacking.caption(string.format('%s %.0f', mark[3], mark[1]), 270, y, {anchor = {0, 0.5}, color = mark[2]})
    end
    if self.messageTime > 0 then
        graphics2d.drawText(nil, self.message, 0, -200, {size = 52, color = '#FFFF8A84', anchor = {0.5, 0.5}, layer = 4})
    end
end

return Stacking
