-- A character that fires a hook toward the pointer or along the aim of a stick, hangs from the ceiling on a distance joint with a limit, swings, reels in and out and lets go.
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local input = require('haylen.input')
local physics2d = require('haylen.physics2d')
local ui = require('haylen.ui')

local parts = require('categories.physics.parts')
local PhysicsTest = require('categories.physics.physics-test')

local GrapplingHook = haylen.class('GrapplingHook', PhysicsTest)

GrapplingHook.actions = {
    {name = 'move', type = 'vector', up = {'key:w', 'key:up', 'button:dpadUp'}, down = {'key:s', 'key:down', 'button:dpadDown'}, left = {'key:a', 'key:left', 'button:dpadLeft'}, right = {'key:d', 'key:right', 'button:dpadRight'}, bindings = {'stick:left', 'virtualStick:move'}},
    {name = 'jump', type = 'button', bindings = {'key:space', 'button:south', 'virtual:jump'}},
    {name = 'hook', type = 'button', bindings = {'key:f', 'button:north', 'virtual:hook'}},
}

local kSolid, kHero = 1, 2
local kSpeed = 380
local kJumpSpeed = 820
local kSwingForce = 900
local kReelSpeed = 420
local kReach = 760
local kHookSpeed = 2600
local kShortest = 40
local kBlocks = {{0, -410, 1600, 40}, {-450, -150, 180, 40}, {0, -240, 220, 40}, {420, -110, 180, 40}, {640, -300, 140, 40}, {620, 120, 300, 30}, {0, 410, 1600, 40}, {-790, 0, 20, 860}, {790, 0, 20, 860}}

function GrapplingHook:enter()
    self:frame{
        hint = 'Walk with A and D, the arrows or the left stick and jump with Space or the south button. Click, tap or press the right trigger to fire the hook at the pointer, or press F or the north button to fire it up ahead. While hanging, W and S reel in and out, sideways input swings and jump lets go.',
        controls = {
            ui.button{id = 'release', text = 'Let go', onClick = function() self:release() end},
            ui.button{id = 'reset', text = 'Start over', onClick = function() self:build() end},
        },
        play = true,
        actions = GrapplingHook.actions,
        overlay = {
            ui.touchStick{action = 'move', radius = 110, mode = 'floating', touchOnly = true, anchor = 'bottomLeft', margin = {0, 0, 110, 40}, width = 360, height = 300},
            ui.touchButton{action = 'hook', text = 'Hook', size = 140, touchOnly = true, anchor = 'bottomRight', margin = {0, 700, 110, 0}},
            ui.touchButton{action = 'jump', text = 'Jump', size = 140, touchOnly = true, anchor = 'bottomRight', margin = {0, 540, 110, 0}},
        },
    }
    self:build()
end

function GrapplingHook:build()
    self.world = physics2d.newWorld()
    self.statics, self.crates = {}, {}
    for _, block in ipairs(kBlocks) do
        local body = self.world:createBody({type = 'static', x = block[1], y = block[2]})
        parts.box(body, block[3], block[4], {category = kSolid})
        self.statics[#self.statics + 1] = body
    end
    for index, x in ipairs({-200, 250, 620}) do
        local crate = self.world:createBody({x = x, y = index == 3 and 70 or 360})
        parts.box(crate, 56, 56, {category = kSolid, friction = 0.8})
        parts.paint(crate, '#FFFFB74D')
        self.crates[#self.crates + 1] = crate
    end
    self.hero = self.world:createBody({x = -650, y = 340, fixedRotation = true, sleepEnabled = false})
    parts.capsule(self.hero, 0, -22, 0, 22, 18, {friction = 0, category = kHero})
    parts.paint(self.hero, '#FF4DD0E1')
    self.facing = 1
    self.hook, self.rope = nil, nil
end

-- Sends the hook out along a direction, and the ray finds where it will catch before it flies there.
function GrapplingHook:fire(dx, dy)
    local length = math.sqrt(dx * dx + dy * dy)
    if length < 1 then
        return
    end
    self:release()
    dx, dy = dx / length, dy / length
    local x, y = self.hero.x, self.hero.y
    local hit = self.world:raycast(x, y, x + dx * kReach, y + dy * kReach, {mask = kSolid})
    self.hook = {dx = dx, dy = dy, travel = 0, distance = hit and hit.distance or kReach, hit = hit}
end

-- Ties a rope from the point the hook caught to the character, as long as the distance between them now.
function GrapplingHook:attach(hit)
    local body, x, y = hit.body, hit.x, hit.y
    local cos, sin = math.cos(-body.rotation), math.sin(-body.rotation)
    local lx, ly = x - body.x, y - body.y
    local length = math.max(kShortest, math.sqrt((x - self.hero.x) ^ 2 + (y - self.hero.y) ^ 2))
    local joint = self.world:createJoint('distance', body, self.hero, {ax = x, ay = y, bx = self.hero.x, by = self.hero.y, enableSpring = true, enableLimit = true, lower = 20, upper = length})
    self.rope = {joint = joint, body = body, localX = lx * cos - ly * sin, localY = lx * sin + ly * cos, length = length}
end

function GrapplingHook:release()
    if self.rope then
        self.rope.joint:destroy()
    end
    self.rope, self.hook = nil, nil
end

function GrapplingHook:anchor()
    local rope = self.rope
    local cos, sin = math.cos(rope.body.rotation), math.sin(rope.body.rotation)
    return rope.body.x + rope.localX * cos - rope.localY * sin, rope.body.y + rope.localX * sin + rope.localY * cos
end

function GrapplingHook:grounded()
    if self.hero.velocity.y < -50 then
        return false
    end
    local x, y = self.hero.x, self.hero.y
    for _, offset in ipairs({-12, 0, 12}) do
        if self.world:raycast(x + offset, y + 38, x + offset, y + 48, {mask = kSolid}) then
            return true
        end
    end
    return false
end

function GrapplingHook:exit()
    GrapplingHook.super.exit(self)
    input.clearVirtual()
end

function GrapplingHook:update(dt)
    GrapplingHook.super.update(self, dt)
    if input.pressed('reset') then
        self:build()
    end
    local moveX, moveY = input.vector('move')
    if math.abs(moveX) > 0.2 then
        self.facing = moveX > 0 and 1 or -1
    end
    if self.pointer.pressed and not ui.usingPointer() then
        self:fire(self.pointer.worldX - self.hero.x, self.pointer.worldY - self.hero.y)
    elseif input.pressed('hook') then
        self:fire(moveY < -0.5 and 0 or self.facing * 0.7, -1)
    end

    local grounded = self:grounded()
    local velocity = self.hero.velocity
    if input.pressed('jump') then
        if self.rope then
            self:release()
            velocity.y = velocity.y - kJumpSpeed * 0.4
        elseif grounded then
            velocity.y = -kJumpSpeed
        end
    end
    if self.rope then
        self.rope.length = math.max(kShortest, math.min(kReach, self.rope.length + moveY * kReelSpeed * dt))
        self.rope.joint.upper = self.rope.length
    elseif grounded then
        velocity.x = moveX * kSpeed
    elseif math.abs(moveX) > 0.1 then
        velocity.x = velocity.x + (moveX * kSpeed - velocity.x) * math.min(1, 3 * dt)
    end
    self.hero.velocity = velocity

    local state = self.rope and string.format('Hanging on %.0f units of rope', self.rope.length) or self.hook and 'Hook flying' or grounded and 'On the ground' or 'In the air'
    self:status(string.format('%s, speed %.0f units per second, step %.2f ms', state, velocity:length(), self:stepTime()))
end

function GrapplingHook:fixedUpdate(step)
    local hook = self.hook
    if hook and not self.rope then
        hook.travel = hook.travel + kHookSpeed * step
        if hook.travel >= hook.distance then
            if hook.hit then
                self:attach(hook.hit)
            else
                self.hook = nil
            end
        end
    end
    if self.rope then
        local moveX = input.vector('move')
        self.hero:applyForce(moveX * self.hero.mass * kSwingForce, 0)
    end
    self:simulate(self.world, step)
end

function GrapplingHook:draw(area)
    parts.drawAll(self.statics)
    parts.drawAll(self.crates)
    local x, y = self.hero.x, self.hero.y
    if self.rope then
        local ax, ay = self:anchor()
        graphics2d.drawLine(x, y, ax, ay, 4, '#FFD7CCC8', {layer = 1})
        graphics2d.drawCircle(ax, ay, 8, '#FFFFD54F', {layer = 2})
    elseif self.hook then
        local travel = math.min(self.hook.travel, self.hook.distance)
        local tx, ty = x + self.hook.dx * travel, y + self.hook.dy * travel
        graphics2d.drawLine(x, y, tx, ty, 3, '#FFD7CCC8', {layer = 1})
        graphics2d.drawCircle(tx, ty, 8, '#FFFFD54F', {layer = 2})
    end
    parts.draw(self.hero, {layer = 3})
    graphics2d.drawCircle(x + self.facing * 8, y - 14, 4, '#FF263238', {layer = 4})
end

return GrapplingHook
