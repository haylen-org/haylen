-- Fast bodies side by side in two worlds: on the left cannons shoot balls through a thin steel wall and a thin moving plate and crates fall through a thin shelf, and on the right continuous collision stops them at the static wall and the shelf and `bullet = true` stops them at the moving plate.
local assets = require('haylen.assets')
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local input = require('haylen.input')
local physics2d = require('haylen.physics2d')
local ui = require('haylen.ui')

local Look = require('categories.physics.look')
local PhysicsTest = require('categories.physics.physics-test')
local Scenery = require('categories.physics.scenery')

local Tunneling = haylen.class('Tunneling', PhysicsTest)

Tunneling.code = [[
local world = physics2d.newWorld({continuous = true})  -- Fast bodies stop at thin static walls.
local ball = world:createBody({x = -330, y = 0, vx = 6000, gravityScale = 0, bullet = true})  -- Also stops at moving bodies.
ball:addCircle(15)
wall:addSegment(210, -240, 210, -100)]]

Tunneling.interval = 0.6
Tunneling.lifetime = 1.6
Tunneling.fade = 0.5
Tunneling.wallX = 210
Tunneling.cannonX = -300
Tunneling.staticLane, Tunneling.movingLane, Tunneling.shelfY = -170, 10, 250
Tunneling.ground = 350
Tunneling.names = {'The wall', 'The moving plate', 'The shelf'}

function Tunneling:enter()
    self:frame{
        code = Tunneling.code,
        hint = 'Every 0.6 seconds both sides fire a ball at a thin steel wall and a ball at a thin moving plate and drop a crate onto a thin shelf. Raise the speed to see more of them pass on the left. R or the X button starts over.',
        controls = {
            ui.label{text = 'Speed of the shots', font = 'caption', color = 'textMuted'},
            ui.slider{id = 'speed', value = 6000, min = 1000, max = 12000, step = 500, showValue = true, decimals = 0, onChange = function(event) self.speed = event.value end},
            ui.button{id = 'shoot', text = 'Shoot now', onClick = function() self:shoot() end},
            ui.button{id = 'reset', text = 'Start over', onClick = function() self:build() end},
        },
        pointer = false,
        focus = 'speed',
    }
    self.speed = 6000
    self.cannon = assets.texture('physics/cannon.png', {filter = 'linear'})
    self:build()
end

-- The left world has no continuous collision and fires plain bodies, and the right world keeps it on and fires bullets.
function Tunneling:build()
    self.halves = {}
    for index, fixed in ipairs({false, true}) do
        local origin = index == 1 and -400 or 400
        local world = physics2d.newWorld({continuous = fixed})
        local ground = world:createBody({type = 'static', x = origin, y = Tunneling.ground + 30})
        ground:addBox(800, 60)
        local wall = world:createBody({type = 'static'})
        wall:addSegment(origin + Tunneling.wallX, Tunneling.staticLane - 70, origin + Tunneling.wallX, Tunneling.staticLane + 70)
        Look.dress(wall, 'steel', {thickness = 5})
        local shelf = world:createBody({type = 'static'})
        shelf:addSegment(origin - 150, Tunneling.shelfY, origin + 150, Tunneling.shelfY)
        Look.dress(shelf, 'wood', {thickness = 6})
        local hinge = world:createBody({type = 'static', x = origin + Tunneling.wallX, y = Tunneling.movingLane - 90})
        local plate = world:createBody({x = origin + Tunneling.wallX, y = Tunneling.movingLane})
        plate:addBox(5, 160, {density = 20})
        Look.dress(plate, 'steel')
        world:createJoint('revolute', hinge, plate, {ax = origin + Tunneling.wallX, ay = Tunneling.movingLane - 80, enableSpring = true, hertz = 2, dampingRatio = 0.5})
        self.halves[index] = {world = world, origin = origin, bullet = fixed, statics = {wall, shelf, plate}, plate = plate, shots = {}, through = {0, 0, 0}, stopped = {0, 0, 0}}
    end
    self.clock = 0
end

function Tunneling:shoot()
    for _, half in ipairs(self.halves) do
        local world, origin = half.world, half.origin
        local x = origin + Tunneling.cannonX + 50
        local ball = world:createBody({x = x, y = Tunneling.staticLane, vx = self.speed, gravityScale = 0, bullet = half.bullet})
        ball:addCircle(15)
        Look.dress(ball, 'iron')
        local fast = world:createBody({x = x, y = Tunneling.movingLane, vx = self.speed, gravityScale = 0, bullet = half.bullet})
        fast:addCircle(15)
        Look.dress(fast, 'iron')
        local crate = world:createBody({x = origin, y = Tunneling.shelfY - 200, vy = self.speed, bullet = half.bullet})
        crate:addBox(40, 40)
        Look.dress(crate, 'woodCrate')
        for lane, body in ipairs({ball, fast, crate}) do
            half.shots[#half.shots + 1] = {body = body, lane = lane, age = 0}
        end
    end
end

-- A shot counts once: as through when it gets past its wall or shelf, and as stopped when it comes to rest or its time runs out first. A counted shot fades away.
function Tunneling:count(half, step)
    for index = #half.shots, 1, -1 do
        local shot = half.shots[index]
        shot.age = shot.age + step
        local body = shot.body
        if shot.counted then
            shot.counted = shot.counted + step
            if shot.counted > Tunneling.fade then
                body:destroy()
                table.remove(half.shots, index)
            end
        else
            local past = shot.lane == 3 and body.y > Tunneling.shelfY + 20 or shot.lane ~= 3 and body.x > half.origin + Tunneling.wallX + 20
            local resting = shot.age > 0.3 and body.velocity:length() < 30
            if past or resting or shot.age > Tunneling.lifetime then
                local tally = past and half.through or half.stopped
                tally[shot.lane] = tally[shot.lane] + 1
                shot.counted = 0
                if shot.lane ~= 3 then
                    body.velocity = {0, 0}
                end
            end
        end
    end
end

function Tunneling:update(dt)
    Tunneling.super.update(self, dt)
    if input.pressed('reset') then
        self:build()
    end
    local left, right = self.halves[1], self.halves[2]
    self:status(string.format('Through on the left: %d balls past the wall, %d past the plate and %d crates past the shelf. On the right: %d, %d and %d.', left.through[1], left.through[2], left.through[3], right.through[1], right.through[2], right.through[3]))
end

function Tunneling:fixedUpdate(step)
    self.clock = self.clock + step
    if self.clock >= Tunneling.interval then
        self.clock = self.clock - Tunneling.interval
        self:shoot()
    end
    for _, half in ipairs(self.halves) do
        self:simulate(half.world, step)
        self:count(half, step)
    end
end

-- A shot draws with a streak behind it as long as the way it moved in the last frame, and fades once it is counted.
function Tunneling:drawShot(shot)
    local body = shot.body
    if not body.valid then
        return
    end
    local velocity = body.velocity
    local speed = velocity:length()
    if shot.lane ~= 3 and speed > 200 and not shot.counted then
        local length = math.min(speed / 60, 160)
        local dx, dy = velocity.x / speed, velocity.y / speed
        graphics2d.drawStroke({body.x - dx * length, body.y - dy * length, body.x, body.y}, {widths = {0, 18}, colors = {'#00FFE08A', '#CCFFE08A'}, cap = 'round', layer = 1})
    end
    if shot.counted then
        graphics2d.beginGroup({opacity = 1 - shot.counted / Tunneling.fade, layer = 2})
        Look.draw(body, {layer = 2})
        graphics2d.endGroup()
    else
        Look.draw(body, {layer = 2})
    end
end

function Tunneling:draw(area)
    Scenery.sky(self.camera, Tunneling.ground)
    Scenery.ground(Tunneling.ground)
    self:compare('Without continuous collision', 'Continuous collision and bullets', 0, -335)
    for _, half in ipairs(self.halves) do
        local origin = half.origin
        Look.drawAll(half.statics, {layer = 1})
        for _, y in ipairs({Tunneling.staticLane, Tunneling.movingLane}) do
            graphics2d.draw(self.cannon, origin + Tunneling.cannonX, y, {width = 100, height = 52, layer = 3})
        end
        Look.block(origin - 160, (Tunneling.shelfY + Tunneling.ground) / 2, 16, Tunneling.ground - Tunneling.shelfY, 'darkWood', 0)
        Look.block(origin + 160, (Tunneling.shelfY + Tunneling.ground) / 2, 16, Tunneling.ground - Tunneling.shelfY, 'darkWood', 0)
        Look.pin(origin + Tunneling.wallX, Tunneling.movingLane - 80, 6 * Look.pixel, 3)
        for _, shot in ipairs(half.shots) do
            self:drawShot(shot)
        end
        for lane, spot in ipairs({{Tunneling.wallX - 10, Tunneling.staticLane - 100}, {Tunneling.wallX - 10, Tunneling.movingLane + 110}, {0, Tunneling.shelfY + 52}}) do
            self:label(string.format('%s: %d through, %d stopped', Tunneling.names[lane], half.through[lane], half.stopped[lane]), origin + spot[1], spot[2], {size = 17})
        end
    end
end

return Tunneling
