-- A hero who fires a grappling hook toward the pointer or along the aim of a stick, hangs from the stone ceiling on a distance joint with a limit, swings, reels in and out and lets go. The hero opens hanging and swinging.
local haylen = require('haylen')
local input = require('haylen.input')
local physics2d = require('haylen.physics2d')
local ui = require('haylen.ui')

local Hero = require('categories.physics.hero')
local Look = require('categories.physics.look')
local PhysicsTest = require('categories.physics.physics-test')
local Scenery = require('categories.physics.scenery')

local GrapplingHook = haylen.class('GrapplingHook', PhysicsTest)

GrapplingHook.code = [[
local hit = world:raycast(hero.x, hero.y, hero.x + dx * 760, hero.y + dy * 760, {mask = kSolid})
-- A spring of 0 hertz with a limit is a rope that goes slack.
local rope = world:createJoint('distance', hit.body, hero, {ax = hit.x, ay = hit.y, bx = hero.x, by = hero.y, enableSpring = true, enableLimit = true, lower = 20, upper = hit.distance})
rope.upper = rope.upper + moveY * 420 * dt  -- Reels in and out.
hero:applyForce(moveX * hero.mass * 900, 0)  -- Swings.
rope:destroy()  -- Lets go.]]

GrapplingHook.actions = {
    {name = 'move', type = 'vector', up = {'key:w', 'key:up', 'button:dpadUp'}, down = {'key:s', 'key:down', 'button:dpadDown'}, left = {'key:a', 'key:left', 'button:dpadLeft'}, right = {'key:d', 'key:right', 'button:dpadRight'}, bindings = {'stick:left', 'virtualStick:move'}},
    {name = 'jump', type = 'button', bindings = {'key:space', 'button:south', 'virtual:jump'}},
    {name = 'hook', type = 'button', bindings = {'key:f', 'button:north', 'virtual:hook'}},
}

GrapplingHook.solid, GrapplingHook.heroCategory = 1, 2
GrapplingHook.speed = 380
GrapplingHook.jumpSpeed = 820
GrapplingHook.swingForce = 900
GrapplingHook.reelSpeed = 420
GrapplingHook.reach = 760
GrapplingHook.hookSpeed = 2600
GrapplingHook.shortest = 40
GrapplingHook.ground = 360
GrapplingHook.radius, GrapplingHook.height = 18, 80
-- The ceiling, the ledges, the ground and the walls, as the middle and the size of each block.
GrapplingHook.blocks = {{0, -350, 1640, 40}, {-450, -110, 180, 36}, {0, -200, 220, 36}, {420, -70, 180, 36}, {640, -250, 140, 36}, {620, 140, 300, 30}, {0, GrapplingHook.ground + 30, 8000, 60}, {-800, 0, 30, 760}, {800, 0, 30, 760}}

function GrapplingHook:enter()
    self:frame{
        code = GrapplingHook.code,
        hint = 'Click, tap or press the right trigger to fire the hook at the pointer, or F or the north button to fire it up ahead.',
        controls = {
            ui.label{text = 'Walk with A and D or the left stick, and jump with Space or the south button.', font = 'caption', color = 'textMuted'},
            ui.label{text = 'While hanging, W and S reel the rope in and out, sideways swings and jumping lets go.', font = 'caption', color = 'textMuted'},
            ui.button{id = 'release', text = 'Let go', onClick = function() self:release() end},
            ui.button{id = 'reset', text = 'Start over', onClick = function() self:build() end},
        },
        play = true,
        actions = GrapplingHook.actions,
        overlay = {
            ui.touchStick{action = 'move', radius = 110, mode = 'floating', touchOnly = true, anchor = 'bottomLeft', anchorTo = 'parent', margin = {0, 0, 24, 24}, width = 360, height = 300},
            ui.touchButton{action = 'hook', text = 'Hook', size = 140, touchOnly = true, anchor = 'bottomRight', anchorTo = 'parent', margin = {0, 184, 24, 0}},
            ui.touchButton{action = 'jump', text = 'Jump', size = 140, touchOnly = true, anchor = 'bottomRight', anchorTo = 'parent', margin = {0, 24, 24, 0}},
        },
    }
    self.heroArt = Hero(GrapplingHook.height)
    self.rope = {0, 0, 0, 0}
    self:build()
end

function GrapplingHook:build()
    self.world = physics2d.newWorld()
    self.statics, self.crates = {}, {}
    for index, block in ipairs(GrapplingHook.blocks) do
        local body = self.world:createBody({type = 'static', x = block[1], y = block[2]})
        body:addBox(block[3], block[4], {category = GrapplingHook.solid})
        if index ~= 7 then
            Look.dress(body, 'slate')
            self.statics[#self.statics + 1] = body
        end
    end
    for index, x in ipairs({-200, 250, 620}) do
        local crate = self.world:createBody({x = x, y = index == 3 and 90 or GrapplingHook.ground - 30})
        crate:addBox(58, 58, {category = GrapplingHook.solid, friction = 0.8})
        Look.dress(crate, 'woodCrate')
        self.crates[#self.crates + 1] = crate
    end
    local reach = GrapplingHook.height / 2 - GrapplingHook.radius
    self.hero = self.world:createBody({x = -520, y = -10, fixedRotation = true, sleepEnabled = false})
    self.hero:addCapsule(0, -reach, 0, reach, GrapplingHook.radius, {friction = 0, category = GrapplingHook.heroCategory})
    self.facing = 1
    self.line, self.hook = nil, nil

    -- The hero starts hanging from the ceiling and swinging.
    local hit = self.world:raycast(-520, -10, -300, -400, {mask = GrapplingHook.solid})
    if hit then
        self:attach(hit)
        self.hero.velocity = {420, 0}
    end
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
    local reach = GrapplingHook.reach
    local hit = self.world:raycast(x, y, x + dx * reach, y + dy * reach, {mask = GrapplingHook.solid})
    self.hook = {dx = dx, dy = dy, travel = 0, distance = hit and hit.distance or reach, hit = hit}
end

-- Ties a rope from the point the hook caught to the hero, as long as the distance between them now.
function GrapplingHook:attach(hit)
    local body, x, y = hit.body, hit.x, hit.y
    local cos, sin = math.cos(-body.rotation), math.sin(-body.rotation)
    local lx, ly = x - body.x, y - body.y
    local length = math.max(GrapplingHook.shortest, math.sqrt((x - self.hero.x) ^ 2 + (y - self.hero.y) ^ 2))
    local joint = self.world:createJoint('distance', body, self.hero, {ax = x, ay = y, bx = self.hero.x, by = self.hero.y, enableSpring = true, enableLimit = true, lower = 20, upper = length})
    self.line = {joint = joint, body = body, localX = lx * cos - ly * sin, localY = lx * sin + ly * cos, length = length}
end

function GrapplingHook:release()
    if self.line then
        self.line.joint:destroy()
    end
    self.line, self.hook = nil, nil
end

function GrapplingHook:anchor()
    local line = self.line
    local cos, sin = math.cos(line.body.rotation), math.sin(line.body.rotation)
    return line.body.x + line.localX * cos - line.localY * sin, line.body.y + line.localX * sin + line.localY * cos
end

function GrapplingHook:grounded()
    if self.hero.velocity.y < -50 then
        return false
    end
    local x, y = self.hero.x, self.hero.y + GrapplingHook.height / 2
    for _, offset in ipairs({-12, 0, 12}) do
        if self.world:raycast(x + offset, y - 4, x + offset, y + 8, {mask = GrapplingHook.solid}) then
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
        if self.line then
            self:release()
            velocity.y = velocity.y - GrapplingHook.jumpSpeed * 0.4
        elseif grounded then
            velocity.y = -GrapplingHook.jumpSpeed
        end
    end
    if self.line then
        self.line.length = math.max(GrapplingHook.shortest, math.min(GrapplingHook.reach, self.line.length + moveY * GrapplingHook.reelSpeed * dt))
        self.line.joint.upper = self.line.length
    elseif grounded then
        velocity.x = moveX * GrapplingHook.speed
    elseif math.abs(moveX) > 0.1 then
        velocity.x = velocity.x + (moveX * GrapplingHook.speed - velocity.x) * math.min(1, 3 * dt)
    end
    self.hero.velocity = velocity
    self.heroArt:update(dt, grounded and velocity.x or 0, grounded)
    if not grounded and math.abs(velocity.x) > 40 then
        self.heroArt.facing = velocity.x > 0 and 1 or -1
    end

    local speed = math.floor(velocity:length() + 0.5)
    local state = self.line and string.format('hangs on %d units of rope', math.floor(self.line.length + 0.5)) or self.hook and 'fires the hook' or grounded and 'stands on the ground' or 'flies through the air'
    self:status(string.format('The hero %s at %d per second.', state, speed))
end

function GrapplingHook:fixedUpdate(step)
    local hook = self.hook
    if hook and not self.line then
        hook.travel = hook.travel + GrapplingHook.hookSpeed * step
        if hook.travel >= hook.distance then
            if hook.hit then
                self:attach(hook.hit)
            else
                self.hook = nil
            end
        end
    end
    if self.line then
        local moveX = input.vector('move')
        self.hero:applyForce(moveX * self.hero.mass * GrapplingHook.swingForce, 0)
    end
    self:simulate(self.world, step)
end

-- The hook is a steel claw of three prongs pointing the way it flies, on a braided rope.
function GrapplingHook:drawHook(hx, hy, tx, ty)
    local dx, dy = tx - hx, ty - hy
    local length = math.max(math.sqrt(dx * dx + dy * dy), 1)
    local ux, uy = dx / length, dy / length
    local unit = Look.pixel
    self.rope[1], self.rope[2], self.rope[3], self.rope[4] = hx, hy, tx, ty
    Look.rope(self.rope, 8, 1)
    for _, spread in ipairs({-0.9, 0, 0.9}) do
        local cos, sin = math.cos(spread), math.sin(spread)
        local px, py = ux * cos - uy * sin, ux * sin + uy * cos
        Look.line(tx, ty, tx + px * 18, ty + py * 18, 5 * unit, '#FFC9D3E2', 2)
    end
    Look.pin(tx, ty, 7 * unit, 3, '#FFA9BCD3')
end

function GrapplingHook:draw(area)
    Scenery.sky(self.camera, GrapplingHook.ground)
    Scenery.ground(GrapplingHook.ground)
    Look.drawAll(self.statics)
    Look.drawAll(self.crates)
    local hero = self.hero
    local hand = hero.y - GrapplingHook.height * 0.2
    local rotation = 0
    if self.line then
        local ax, ay = self:anchor()
        self:drawHook(hero.x, hand, ax, ay)
        rotation = math.atan(ax - hero.x, hero.y - ay) * 0.6
    elseif self.hook then
        local travel = math.min(self.hook.travel, self.hook.distance)
        self:drawHook(hero.x, hand, hero.x + self.hook.dx * travel, hero.y + self.hook.dy * travel)
    end
    self.heroArt:draw(hero.x, hero.y + GrapplingHook.height / 2, 3, rotation)
end

return GrapplingHook
