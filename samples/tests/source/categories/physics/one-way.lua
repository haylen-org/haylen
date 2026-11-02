-- One-way platforms on wooden posts: the hero jumps up through them and lands on them, drops down through them with `body:dropThrough`, and rides a lift that moves up and down between steel rails.
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local input = require('haylen.input')
local physics2d = require('haylen.physics2d')
local ui = require('haylen.ui')

local Hero = require('categories.physics.hero')
local Look = require('categories.physics.look')
local PhysicsTest = require('categories.physics.physics-test')
local Scenery = require('categories.physics.scenery')

local OneWay = haylen.class('OneWay', PhysicsTest)

OneWay.code = [[
platform:addBox(300, 22, {oneWay = {0, -1}, friction = 0.8})  -- Solid only from above.
hero:dropThrough(0.3)  -- Falls through the platform it stands on.
shape.oneWay = nil  -- Makes the platform solid from every side.
lift.velocity = {0, math.cos(time * 0.8) * 120}  -- A lift that the hero rides.]]

OneWay.heroCategory = 1
OneWay.solidCategory = 2
OneWay.oneWayCategory = 4
OneWay.speed = 380
OneWay.jumpSpeed = 1000
OneWay.dropTime = 0.3
OneWay.groundY = 360
OneWay.thickness = 22
-- Each platform with its middle, its height and its width, one jump above the one before it.
OneWay.platforms = {{-440, 221, 300}, {-40, 71, 300}, {360, -79, 280}}
OneWay.lift = {640, 71, 200}
OneWay.liftTravel = 150
OneWay.radius, OneWay.height = 24, 112
OneWay.wallX, OneWay.wallTop = 780, -330

OneWay.actions = {
    {name = 'move', type = 'vector', up = {'key:w', 'key:up', 'button:dpadUp'}, down = {'key:s', 'key:down', 'button:dpadDown'}, left = {'key:a', 'key:left', 'button:dpadLeft'}, right = {'key:d', 'key:right', 'button:dpadRight'}, bindings = {'stick:left', 'virtualStick:move'}},
    {name = 'jump', type = 'button', bindings = {'key:space', 'key:w', 'key:up', 'button:south', 'virtual:jump'}},
}

function OneWay:enter()
    self:frame{
        code = OneWay.code,
        hint = 'Move with A and D, the arrows or the left stick, jump with Space, W or the south button, and hold down while jumping to drop through a platform. Climb up to the top platform, or ride the lift.',
        controls = {
            ui.checkbox{id = 'solid', text = 'Make the platforms solid', onChange = function(event) self:setSolid(event.checked) end},
            ui.button{id = 'reset', text = 'Start over', onClick = function() self:build() end},
        },
        play = true,
        pointer = false,
        actions = OneWay.actions,
        overlay = {
            ui.touchStick{action = 'move', radius = 110, mode = 'floating', touchOnly = true, anchor = 'bottomLeft', anchorTo = 'parent', margin = {0, 0, 24, 24}, width = 360, height = 300},
            ui.touchButton{action = 'jump', text = 'Jump', size = 150, touchOnly = true, anchor = 'bottomRight', anchorTo = 'parent', margin = {0, 24, 24, 0}},
        },
    }
    self.heroArt = Hero(OneWay.height)
    self.chevron = {0, 0, 0, 0, 0, 0}
    self.chevronStroke = {width = 1, color = '#B3FFFFFF', join = 'round', cap = 'round', layer = 2}
    self:build()
end

function OneWay:build()
    self.world = physics2d.newWorld({owner = self, gravity = {0, 2200}})
    self.platforms = {}
    self.time = 0

    -- The walls reach far above the stage, so the hero never jumps over them, while only their pillars up to the caps show.
    local ground = self.world:createBody({type = 'static', x = 0, y = OneWay.groundY + 30})
    ground:addBox(8000, 60, {category = OneWay.solidCategory})
    for _, side in ipairs({-1, 1}) do
        ground:addBox(40, 2000, {offsetX = side * OneWay.wallX, offsetY = -1030, category = OneWay.solidCategory})
    end

    -- Each platform lies one jump above the one before it and less than a jump away to the side, zigzagging up to the top one.
    for _, spot in ipairs(OneWay.platforms) do
        self:platform('static', spot[1], spot[2], spot[3])
    end
    self.top = self.platforms[#self.platforms].body
    self.lift = self:platform('kinematic', OneWay.lift[1], OneWay.lift[2], OneWay.lift[3])

    local reach = OneWay.height / 2 - OneWay.radius
    self.hero = self.world:createBody({x = -660, y = OneWay.groundY - OneWay.height / 2 - 4, fixedRotation = true, sleepEnabled = false})
    self.hero:addCapsule(0, -reach, 0, reach, OneWay.radius, {friction = 0, category = OneWay.heroCategory})
end

function OneWay:platform(kind, x, y, width)
    local body = self.world:createBody({type = kind, x = x, y = y})
    local shape = body:addBox(width, OneWay.thickness, {category = OneWay.oneWayCategory, oneWay = {0, -1}, friction = 0.8})
    Look.dress(body, kind == 'static' and 'wood' or 'steel')
    self.platforms[#self.platforms + 1] = {body = body, shape = shape, width = width}
    return body
end

function OneWay:setSolid(solid)
    for _, platform in ipairs(self.platforms) do
        platform.shape.oneWay = not solid and {0, -1} or nil
    end
end

-- Returns the body the hero stands on, which it stands on when it is not rising and a short ray from its feet meets the ground or a platform.
function OneWay:ground()
    if self.hero.velocity.y < -50 then
        return nil
    end
    local x, y = self.hero.x, self.hero.y + OneWay.height / 2
    for _, offset in ipairs({-14, 0, 14}) do
        local hit = self.world:raycast(x + offset, y - 2, x + offset, y + 8, {mask = OneWay.solidCategory | OneWay.oneWayCategory})
        if hit then
            return hit.body
        end
    end
    return nil
end

function OneWay:exit()
    OneWay.super.exit(self)
    input.clearVirtual()
end

-- Names the place of the hero, counting the platforms from the ground up.
function OneWay:place(ground)
    if not ground then
        return 'The hero is in the air'
    end
    if ground == self.lift then
        return 'The hero rides the lift'
    end
    for index, platform in ipairs(self.platforms) do
        if platform.body == ground then
            return index == #self.platforms - 1 and 'The hero stands on the top platform' or string.format('The hero stands on platform %d of %d', index, #self.platforms - 1)
        end
    end
    return 'The hero stands on the ground'
end

function OneWay:update(dt)
    OneWay.super.update(self, dt)
    if input.pressed('reset') then
        self:build()
    end
    local moveX, moveY = input.vector('move')
    local velocity = self.hero.velocity
    local ground = self:ground()
    if input.pressed('jump') and ground then
        if moveY > 0.5 then
            self.hero:dropThrough(OneWay.dropTime)
        else
            velocity.y = -OneWay.jumpSpeed
        end
    end
    -- The hero keeps the speed of the platform it stands on, so it rides the moving one.
    local carried = ground and ground.velocity.x or 0
    self.hero.velocity = {moveX * OneWay.speed + carried, velocity.y}
    self.heroArt:update(dt, moveX * OneWay.speed, ground ~= nil)
    self:status(self:place(ground) .. '.')
end

function OneWay:fixedUpdate(step)
    self.time = self.time + step
    self.lift.velocity = {0, math.cos(self.time * 0.8) * OneWay.liftTravel * 0.8}
    self:simulate(self.world, step)
end

-- A one-way platform shows light chevrons that point the way through it, and a solid one shows none.
function OneWay:drawChevrons(platform)
    local body = platform.body
    local unit = Look.pixel
    local chevron = self.chevron
    self.chevronStroke.width = 3 * unit
    for dx = -platform.width / 2 + 30, platform.width / 2 - 30, 40 do
        chevron[1], chevron[2] = body.x + dx - 7, body.y + 4
        chevron[3], chevron[4] = body.x + dx, body.y - 3
        chevron[5], chevron[6] = body.x + dx + 7, body.y + 4
        graphics2d.drawStroke(chevron, self.chevronStroke)
    end
end

-- The static platforms stand on wooden posts behind them that reach down to the ground, the lift slides between two steel rails, and a stone pillar with a cap stands at each side.
function OneWay:drawFrame()
    local ground = OneWay.groundY
    for _, spot in ipairs(OneWay.platforms) do
        local top = spot[2] + OneWay.thickness / 2
        for _, side in ipairs({-1, 1}) do
            Look.block(spot[1] + side * (spot[3] / 2 - 30), (top + ground) / 2, 16, ground - top, 'darkWood', 0)
        end
    end
    local lift = OneWay.lift
    local railTop = lift[2] - OneWay.liftTravel - OneWay.height - 40
    for _, side in ipairs({-1, 1}) do
        Look.block(lift[1] + side * (lift[3] / 2 + 6), (railTop + ground) / 2, 12, ground - railTop, 'steel', 0)
    end
    Look.block(lift[1], railTop, lift[3] + 40, 16, 'iron', 0)
    for _, side in ipairs({-1, 1}) do
        local x = side * OneWay.wallX
        Look.block(x, (OneWay.wallTop + ground) / 2, 40, ground - OneWay.wallTop, 'stone', 0)
        Look.block(x, OneWay.wallTop - 10, 64, 24, 'stone', 0)
    end
end

function OneWay:draw(area)
    Scenery.sky(self.camera, OneWay.groundY)
    Scenery.ground(OneWay.groundY)
    self:drawFrame()
    for _, platform in ipairs(self.platforms) do
        Look.draw(platform.body, {layer = 1})
        if platform.shape.oneWay then
            self:drawChevrons(platform)
        end
    end
    self:label('The lift', self.lift.x, self.lift.y + 36, {size = 17})
    self.heroArt:draw(self.hero.x, self.hero.y + OneWay.height / 2, 3)
end

return OneWay
