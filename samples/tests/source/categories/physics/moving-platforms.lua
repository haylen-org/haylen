-- The same moving platforms in two worlds: on the left they jump to their next place every step with `body:setTransform`, so they slide out from under their loads and knock them away, and on the right `body:moveTo` gives them the velocity that reaches that place in one step, so friction carries crates and balls and a hero rides them.
local assets = require('haylen.assets')
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local input = require('haylen.input')
local physics2d = require('haylen.physics2d')
local ui = require('haylen.ui')

local Hero = require('categories.physics.hero')
local Look = require('categories.physics.look')
local PhysicsTest = require('categories.physics.physics-test')
local Scenery = require('categories.physics.scenery')

local MovingPlatforms = haylen.class('MovingPlatforms', PhysicsTest)

MovingPlatforms.code = [[
local platform = world:createBody({type = 'kinematic'})
platform:addBox(200, 18, {friction = 0.9})
platform:moveTo(x, y, rotation)  -- Each fixed step, so friction carries its loads.
platform:setTransform(x, y, rotation)  -- A teleport slides out from under them.
local riding = mover.groundBody == platform  -- A mover rides it too.]]

MovingPlatforms.gravity = 1600
MovingPlatforms.radius, MovingPlatforms.height = 20, 90
MovingPlatforms.ground = 360
MovingPlatforms.kinds = {slider = 'the sliding platform', lift = 'the lift', turner = 'the seesaw'}

function MovingPlatforms:enter()
    self:frame{
        code = MovingPlatforms.code,
        hint = 'The platforms on both sides follow the same paths, shown dashed. Drop new loads on them with the button. R or the X button starts over.',
        controls = {
            ui.label{text = 'Speed of the platforms', font = 'caption', color = 'textMuted'},
            ui.slider{id = 'speed', value = 1, min = 0.25, max = 2.5, step = 0.25, showValue = true, decimals = 2, onChange = function(event) self.speed = event.value end},
            ui.button{id = 'drop', text = 'Drop loads', onClick = function() self:drop() end},
            ui.button{id = 'reset', text = 'Start over', onClick = function() self:build() end},
        },
        pointer = false,
        focus = 'speed',
    }
    self.speed = 1
    self.hero = Hero(MovingPlatforms.height)
    self.codeFont = assets.font('fonts/space-mono-regular.ttf', {bakeSize = 64, spread = 16})
    self.codeText = {size = 1, color = '#FFE8EAF2', anchor = {0.5, 0.5}, layer = 41}
    self.codePlate = {kind = 'rectangle', radius = 0, color = '#E6232938', borderWidth = 0, borderColor = '#FF15172A', layer = 40}
    self.codeRect = {0, 0, 0, 0}
    self:build()
end

function MovingPlatforms:build()
    self.halves = {}
    for index = 1, 2 do
        local origin = index == 1 and -400 or 400
        local world = physics2d.newWorld({gravity = {0, MovingPlatforms.gravity}})
        local ground = world:createBody({type = 'static', x = origin, y = MovingPlatforms.ground + 30})
        ground:addBox(800, 60)
        ground:addBox(20, 140, {offsetX = -390, offsetY = -100})
        ground:addBox(20, 140, {offsetX = 390, offsetY = -100})
        local half = {world = world, origin = origin, platforms = {}, loads = {}, lost = 0}
        half.platforms[1] = self:platform(half, 'slider', 200, 18)
        half.platforms[2] = self:platform(half, 'lift', 160, 18)
        half.platforms[3] = self:platform(half, 'turner', 240, 18)
        self.halves[index] = half
    end
    self.time = 0
    self:drop()
    local right = self.halves[2]
    self.mover = physics2d.newMover(right.world, {x = right.origin - 150, y = -240, radius = MovingPlatforms.radius, height = MovingPlatforms.height, snapDistance = 12})
    self.velocity = {x = 0, y = 0}
end

function MovingPlatforms:platform(half, kind, width, height)
    local body = half.world:createBody({type = 'kinematic'})
    body:addBox(width, height, {friction = 0.9})
    Look.dress(body, 'steel')
    local platform = {body = body, kind = kind}
    self:place(half, platform, 0, false)
    return platform
end

-- The paths of the platforms: one slides from side to side, one rises and sinks, one rocks around its middle.
function MovingPlatforms:pathOf(half, kind, time)
    if kind == 'slider' then
        return half.origin - 150 + math.sin(time * 1.3) * 160, -150, 0
    elseif kind == 'lift' then
        return half.origin + 220, 100 + math.sin(time) * 180, 0
    end
    return half.origin - 120, 200, math.sin(time * 0.9) * 0.6
end

function MovingPlatforms:place(half, platform, time, moving)
    local x, y, rotation = self:pathOf(half, platform.kind, time)
    if moving and half == self.halves[2] then
        platform.body:moveTo(x, y, rotation)
    else
        platform.body:setTransform(x, y, rotation)
    end
end

function MovingPlatforms:drop()
    for _, half in ipairs(self.halves) do
        for _, platform in ipairs(half.platforms) do
            local x, y = self:pathOf(half, platform.kind, self.time or 0)
            local crate = half.world:createBody({x = x - 50, y = y - 40})
            crate:addBox(44, 44, {friction = 0.8})
            Look.dress(crate, 'woodCrate')
            local ball = half.world:createBody({x = x + 50, y = y - 40})
            ball:addCircle(18, {friction = 0.8})
            Look.dress(ball, 'yellow', {spin = true})
            half.loads[#half.loads + 1] = {body = crate}
            half.loads[#half.loads + 1] = {body = ball}
        end
    end
end

function MovingPlatforms:update(dt)
    MovingPlatforms.super.update(self, dt)
    if input.pressed('reset') then
        self:build()
    end
    local ground = self.mover.groundBody
    local kind
    for _, platform in ipairs(self.halves[2].platforms) do
        if ground == platform.body then
            kind = MovingPlatforms.kinds[platform.kind]
        end
    end
    self.hero:update(dt, self.mover.groundVelocity.x, self.mover.grounded)
    self:status(string.format('The hero %s. Loads that fell off: %d on the left and %d on the right.', kind and 'rides ' .. kind or 'stands on the ground', self.halves[1].lost, self.halves[2].lost))
end

-- A load that falls to the ground counts as lost once.
function MovingPlatforms:fixedUpdate(step)
    self.time = self.time + step * self.speed
    for _, half in ipairs(self.halves) do
        for _, platform in ipairs(half.platforms) do
            self:place(half, platform, self.time, true)
        end
    end

    local mover = self.mover
    local velocity = mover:clip(0, self.velocity.y + MovingPlatforms.gravity * step)
    mover:move(velocity.x * step, velocity.y * step)
    self.velocity = mover:clip(velocity.x, velocity.y)

    for _, half in ipairs(self.halves) do
        self:simulate(half.world, step)
        for _, load in ipairs(half.loads) do
            if not load.lost and load.body.y > MovingPlatforms.ground - 50 then
                load.lost = true
                half.lost = half.lost + 1
            end
        end
    end
end

-- The paths draw dashed: the line the slider and the lift run along, and the arc the ends of the seesaw rock along.
function MovingPlatforms:drawPaths(half)
    local origin = half.origin
    Look.dashed(origin - 310, -150, origin + 10, -150, '#992F3352', -1)
    Look.dashed(origin + 220, -80, origin + 220, 280, '#992F3352', -1)
    local unit = Look.pixel
    for _, side in ipairs({-1, 1}) do
        graphics2d.drawArc(origin - 120, 200, 120, 3 * unit, (side > 0 and 0 or math.pi) - 0.6, (side > 0 and 0 or math.pi) + 0.6, '#992F3352', {layer = -1})
    end
    Look.block(origin - 120, 200 + (MovingPlatforms.ground - 200) / 2, 18, MovingPlatforms.ground - 200, 'darkWood', -2)
end

-- Draws a call of the API in the monospace font of the code card, on a dark pill at `x`, `y` of the world, which keeps its size on the screen at any zoom.
function MovingPlatforms:codeTag(text, x, y)
    local unit = Look.pixel
    local style = self.codeText
    style.size = 16 * unit
    local width, height = graphics2d.measureText(self.codeFont, text, style)
    local plateWidth, plateHeight = width + 22 * unit, height + 10 * unit
    local rect = self.codeRect
    rect[1], rect[2], rect[3], rect[4] = x - plateWidth / 2, y - plateHeight / 2, plateWidth, plateHeight
    self.codePlate.radius, self.codePlate.borderWidth = plateHeight / 2, 2 * unit
    graphics2d.drawShape(rect, self.codePlate)
    graphics2d.drawText(self.codeFont, text, x, y, style)
end

function MovingPlatforms:draw(area)
    Scenery.sky(self.camera, MovingPlatforms.ground)
    Scenery.ground(MovingPlatforms.ground)
    self:compare('Teleported every step', 'Moved by its velocity', 0, -330)
    for index, half in ipairs(self.halves) do
        self:codeTag(index == 1 and 'body:setTransform(x, y, rotation)' or 'body:moveTo(x, y, rotation)', half.origin, -288)
        self:drawPaths(half)
        for _, platform in ipairs(half.platforms) do
            Look.draw(platform.body, {layer = 1})
        end
        for _, load in ipairs(half.loads) do
            Look.draw(load.body, {layer = 2})
        end
    end
    -- The low walls at the ends of the two sides meet in one post at the middle.
    for _, x in ipairs({-790, 0, 790}) do
        Look.block(x, MovingPlatforms.ground - 100, x == 0 and 40 or 20, 140, 'stone', 0)
    end

    self.hero:draw(self.mover.x, self.mover.y + MovingPlatforms.height / 2, 3)
end

return MovingPlatforms
