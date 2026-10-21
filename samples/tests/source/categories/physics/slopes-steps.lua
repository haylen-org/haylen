-- Two heroes on the same hill and stairs, driven by the same input: on the left a dynamic capsule without friction, as characters often are so they never stick to walls, slides down slopes and stumbles over steps, and on the right a mover from `physics2d.newMover` stands still on slopes, keeps its speed on them, steps up the stairs and stays on the ground going down.
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local input = require('haylen.input')
local physics2d = require('haylen.physics2d')
local ui = require('haylen.ui')

local Hero = require('categories.physics.hero')
local Look = require('categories.physics.look')
local PhysicsTest = require('categories.physics.physics-test')
local Scenery = require('categories.physics.scenery')

local SlopesSteps = haylen.class('SlopesSteps', PhysicsTest)

SlopesSteps.code = [[
local hero = physics2d.newMover(world, {x = 240, y = 100, radius = 16, height = 72, stepHeight = 20, maxSlope = math.rad(50)})
local velocity = hero:clip(move * 320, vy + 1800 * step)
hero:move(velocity.x * step, velocity.y * step)  -- Stands on slopes and climbs steps.
vy = hero:clip(velocity.x, velocity.y).y
print(hero.grounded, hero.groundNormal.x, hero.groundNormal.y)]]

SlopesSteps.actions = {
    {name = 'move', type = 'axis', positive = {'key:d', 'key:right', 'button:dpadRight', 'axis:leftX+', 'virtual:right'}, negative = {'key:a', 'key:left', 'button:dpadLeft', 'axis:leftX-', 'virtual:left'}},
    {name = 'jump', type = 'button', bindings = {'key:space', 'key:w', 'key:up', 'button:south', 'virtual:jump'}},
}

SlopesSteps.gravity = 1800
SlopesSteps.speed = 320
SlopesSteps.jump = 760
SlopesSteps.radius, SlopesSteps.height = 16, 72
SlopesSteps.bottom = 440

-- The ground of both sides relative to the middle of a side: a flat start, a 30 degree hill, a plateau, stairs down and a 60 degree wall that is too steep to walk.
SlopesSteps.outline = {{-380, -420}, {-380, 200}, {-260, 200}, {-60, 85}, {20, 85}, {20, 103}, {60, 103}, {60, 121}, {100, 121}, {100, 139}, {140, 139}, {140, 157}, {180, 157}, {180, 175}, {220, 175}, {220, 200}, {300, 200}, {380, 60}, {380, -420}}
SlopesSteps.hill = {{-380, 200}, {-260, 200}, {-60, 85}, {20, 85}}
SlopesSteps.flat = {{220, 200}, {300, 200}}
SlopesSteps.steps = {{20, 103}, {60, 121}, {100, 139}, {140, 157}, {180, 175}}

function SlopesSteps:enter()
    self:frame{
        code = SlopesSteps.code,
        hint = 'Walk with A and D, the arrows, the left stick or the touch buttons, and jump with Space, W or the south button. Both heroes get the same input. R or the X button starts over.',
        controls = {
            ui.label{text = 'Steepest slope the mover walks', font = 'caption', color = 'textMuted'},
            ui.slider{id = 'slope', value = 50, min = 10, max = 80, step = 5, showValue = true, decimals = 0, onChange = function(event) self.mover.maxSlope = math.rad(event.value) end},
            ui.button{id = 'reset', text = 'Start over', onClick = function() self:build() end},
        },
        play = true,
        pointer = false,
        actions = SlopesSteps.actions,
        view = {1600, 720},
        overlay = {
            ui.touchButton{action = 'left', text = 'Left', size = 130, touchOnly = true, anchor = 'bottomLeft', anchorTo = 'parent', margin = {0, 0, 24, 24}},
            ui.touchButton{action = 'right', text = 'Right', size = 130, touchOnly = true, anchor = 'bottomLeft', anchorTo = 'parent', margin = {0, 0, 24, 174}},
            ui.touchButton{action = 'jump', text = 'Jump', size = 150, touchOnly = true, anchor = 'bottomRight', anchorTo = 'parent', margin = {0, 24, 24, 0}},
        },
    }
    self.camera.position = {0, 60}
    self.heroes = {Hero(SlopesSteps.height), Hero(SlopesSteps.height)}
    self.scenery = {}
    for index = 1, 2 do
        local origin = index == 1 and -400 or 400
        local fill = {}
        for point = 2, #SlopesSteps.outline - 1 do
            fill[#fill + 1] = {origin + SlopesSteps.outline[point][1], SlopesSteps.outline[point][2]}
        end
        fill[#fill + 1] = {origin + 380, SlopesSteps.bottom}
        fill[#fill + 1] = {origin - 380, SlopesSteps.bottom}
        local hill, flat = {}, {}
        for _, point in ipairs(SlopesSteps.hill) do
            hill[#hill + 1] = {origin + point[1], point[2]}
        end
        for _, point in ipairs(SlopesSteps.flat) do
            flat[#flat + 1] = {origin + point[1], point[2]}
        end
        local wall = Look.newShape({{origin + 300, 200}, {origin + 380, 60}, {origin + 380, SlopesSteps.bottom}, {origin + 300, SlopesSteps.bottom}}, 'stone')
        self.scenery[index] = {origin = origin, fill = fill, hill = Scenery.newTerrain(hill, SlopesSteps.bottom), flat = Scenery.newTerrain(flat, SlopesSteps.bottom), wall = wall}
    end
    self.dirt = {layer = -10}
    self:build()
end

function SlopesSteps:build()
    self.halves = {}
    for index = 1, 2 do
        local origin = index == 1 and -400 or 400
        local world = physics2d.newWorld({gravity = {0, SlopesSteps.gravity}})
        local ground = world:createBody({type = 'static', x = origin})
        ground:addChain(SlopesSteps.outline, false, {friction = 0.6})
        self.halves[index] = {world = world, origin = origin}
    end

    local left, right = self.halves[1], self.halves[2]
    local reach = SlopesSteps.height / 2 - SlopesSteps.radius
    self.capsule = left.world:createBody({x = left.origin - 160, y = 100, fixedRotation = true, sleepEnabled = false})
    self.capsule:addCapsule(0, -reach, 0, reach, SlopesSteps.radius, {friction = 0})
    self.mover = physics2d.newMover(right.world, {x = right.origin - 160, y = 100, radius = SlopesSteps.radius, height = SlopesSteps.height, stepHeight = 20, snapDistance = 20, maxSlope = math.rad(50)})
    self.velocity = {x = 0, y = 0}
end

-- The capsule stands when a short ray from its feet meets the ground.
function SlopesSteps:capsuleGrounded()
    local x, y = self.capsule.x, self.capsule.y
    return self.halves[1].world:raycast(x, y + SlopesSteps.height / 2 - 2, x, y + SlopesSteps.height / 2 + 6) ~= nil
end

function SlopesSteps:exit()
    SlopesSteps.super.exit(self)
    input.clearVirtual()
end

function SlopesSteps.describe(speed, grounded)
    if not grounded then
        return 'is in the air'
    end
    if math.abs(speed) < 10 then
        return 'stands still'
    end
    return string.format('moves at %d per second', math.floor(math.abs(speed) + 0.5))
end

function SlopesSteps:update(dt)
    SlopesSteps.super.update(self, dt)
    if input.pressed('reset') then
        self:build()
    end
    if input.pressed('jump') then
        self.jumping = true
    end
    local capsuleGrounded = self:capsuleGrounded()
    self.heroes[1]:update(dt, self.capsule.velocity.x, capsuleGrounded)
    self.heroes[2]:update(dt, self.velocity.x, self.mover.grounded)
    local normal = self.mover.groundNormal
    local slope = math.floor(math.deg(math.atan(math.abs(normal.x), -normal.y)) + 0.5)
    self:status(string.format('The capsule %s, and the mover %s%s.', SlopesSteps.describe(self.capsule.velocity.x, capsuleGrounded), SlopesSteps.describe(self.velocity.x, self.mover.grounded), self.mover.grounded and slope > 0 and string.format(' on a slope of %d degrees', slope) or ''))
end

function SlopesSteps:fixedUpdate(step)
    local move = input.value('move')
    local jump = self.jumping
    self.jumping = false

    -- The capsule takes the input as its horizontal velocity and keeps the vertical one that gravity gives it.
    local capsule = self.capsule
    local vy = capsule.velocity.y
    if jump and self:capsuleGrounded() then
        vy = -SlopesSteps.jump
    end
    capsule.velocity = {move * SlopesSteps.speed, vy}

    -- The mover integrates its own velocity, moves by it and drops the part that pushes into what it hit.
    local mover = self.mover
    local velocity = mover:clip(move * SlopesSteps.speed, self.velocity.y + SlopesSteps.gravity * step)
    if jump and mover.grounded then
        velocity.y = -SlopesSteps.jump
    end
    mover:move(velocity.x * step, velocity.y * step)
    self.velocity = mover:clip(velocity.x, velocity.y)

    for _, half in ipairs(self.halves) do
        self:simulate(half.world, step)
    end
end

function SlopesSteps:draw(area)
    Scenery.sky(self.camera, 200)
    Scenery.ground(200, nil, nil, -12)
    self:compare('A dynamic capsule', 'A mover', 0, -250)
    for _, scenery in ipairs(self.scenery) do
        local origin = scenery.origin
        graphics2d.drawPolygon(scenery.fill, Scenery.dirt, self.dirt)
        Scenery.drawTerrain(scenery.hill, -10)
        Scenery.drawTerrain(scenery.flat, -10)
        for _, step in ipairs(SlopesSteps.steps) do
            Look.block(origin + step[1] + 20, (step[2] + 200) / 2 + 2, 40, 200 - step[2] + 4, 'stone', -8)
        end
        Look.drawShape(scenery.wall, -8)
        Look.block(origin - 392, -110, 24, 640, 'stone', -7)
        Look.block(origin + 392, -110, 24, 640, 'stone', -7)
    end
    local feet = SlopesSteps.height / 2
    self.heroes[1]:draw(self.capsule.x, self.capsule.y + feet, 2)
    self.heroes[2]:draw(self.mover.x, self.mover.y + feet, 2)
end

return SlopesSteps
