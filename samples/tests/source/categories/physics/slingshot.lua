-- A wooden slingshot pulled back with the pointer or aimed with the keys or a stick, whose shot follows the path that `world:predictPath` draws before the launch, at little green critters on towers of wood and stone that pop when hit hard.
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local input = require('haylen.input')
local m = require('haylen.math')
local physics2d = require('haylen.physics2d')
local ui = require('haylen.ui')

local Burst = require('categories.physics.burst')
local Look = require('categories.physics.look')
local PhysicsTest = require('categories.physics.physics-test')
local Scenery = require('categories.physics.scenery')

local Slingshot = haylen.class('Slingshot', PhysicsTest)

Slingshot.code = [[
local shot = world:createBody({type = 'kinematic', x = pouchX + pullX, y = pouchY + pullY})  -- Held in the pouch.
local points, hit = world:predictPath(shot.x, shot.y, -pullX * 7.5, -pullY * 7.5, {steps = 150, radius = 20, mask = kWorld | kTarget})
shot.type = 'dynamic'
shot.velocity = {-pullX * 7.5, -pullY * 7.5}  -- Follows the predicted path.
world.onHit = function(a, b, contact) if contact.speed > 260 then pop(a, b) end end]]

Slingshot.actions = {
    {name = 'aim', type = 'vector', up = {'key:w', 'key:up', 'button:dpadUp'}, down = {'key:s', 'key:down', 'button:dpadDown'}, left = {'key:a', 'key:left', 'button:dpadLeft'}, right = {'key:d', 'key:right', 'button:dpadRight'}, bindings = {'stick:left', 'virtualStick:aim'}},
    {name = 'launch', type = 'button', bindings = {'key:space', 'button:south', 'virtual:launch'}},
}

Slingshot.worldCategory, Slingshot.targetCategory, Slingshot.shotCategory = 1, 2, 4
Slingshot.ground = 390
Slingshot.pouch = {-560, 200}
Slingshot.prongs = {{-592, 192}, {-528, 192}}
Slingshot.fork = {{-46, -12}, {-28, -12}, {-8, 22}, {8, 22}, {28, -12}, {46, -12}, {14, 44}, {-14, 44}}
Slingshot.maxPull = 160
Slingshot.power = 7.5
Slingshot.shotRadius = 20
Slingshot.reloadTime = 1
Slingshot.maxShots = 6
Slingshot.popSpeed = 260
-- Every block with its middle, its size and its material.
Slingshot.blocks = {{250, 330, 22, 120, 'wood'}, {390, 330, 22, 120, 'wood'}, {320, 259, 180, 22, 'wood'}, {270, 198, 22, 100, 'stone'}, {370, 198, 22, 100, 'stone'}, {320, 137, 140, 22, 'stone'}, {560, 360, 60, 60, 'woodCrate'}, {640, 360, 60, 60, 'woodCrate'}, {600, 300, 60, 60, 'stone'}}
Slingshot.targets = {{320, 363}, {320, 222}, {600, 243}}

function Slingshot:enter()
    self:frame{
        code = Slingshot.code,
        hint = 'Drag the shot back from the pouch and let go, or aim with the arrows, WASD or the left stick and launch with Space or the south button. R or the X button builds the towers again.',
        controls = {
            ui.button{id = 'launch', text = 'Launch', onClick = function() self:launch() end},
            ui.checkbox{id = 'path', text = 'Show the predicted path', checked = true, onChange = function(event) self.showPath = event.checked end},
            ui.button{id = 'reset', text = 'Build the towers again', onClick = function() self:build() end},
        },
        play = true,
        actions = Slingshot.actions,
        view = {1500, 640},
        overlay = {
            ui.touchStick{action = 'aim', radius = 110, mode = 'floating', touchOnly = true, anchor = 'bottomLeft', anchorTo = 'parent', margin = {0, 0, 24, 24}, width = 360, height = 300},
            ui.touchButton{action = 'launch', text = 'Launch', size = 150, touchOnly = true, anchor = 'bottomRight', anchorTo = 'parent', margin = {0, 24, 24, 0}},
        },
    }
    self.camera.position = {40, 110}
    self.showPath = true
    self.pops = Burst('explosions/poof', {count = 3, scale = 0.6, layer = 30})
    local fork = {}
    for index, point in ipairs(Slingshot.fork) do
        fork[index] = {Slingshot.pouch[1] + point[1], Slingshot.pouch[2] + point[2] - 8}
    end
    self.fork = Look.newShape(fork, 'darkWood')
    self:build()
end

function Slingshot:build()
    self.world = physics2d.newWorld()
    local ground = self.world:createBody({type = 'static', x = 0, y = Slingshot.ground + 30})
    ground:addBox(8000, 60, {category = Slingshot.worldCategory})

    self.blocks, self.critters, self.shots = {}, {}, {}
    for _, spec in ipairs(Slingshot.blocks) do
        local block = self.world:createBody({x = spec[1], y = spec[2]})
        block:addBox(spec[3], spec[4], {category = Slingshot.worldCategory, density = 0.8})
        Look.dress(block, spec[5])
        self.blocks[#self.blocks + 1] = block
    end
    for _, spot in ipairs(Slingshot.targets) do
        local critter = self.world:createBody({x = spot[1], y = spot[2]})
        critter:addCircle(26, {category = Slingshot.targetCategory, density = 0.5})
        critter.data = {target = true}
        self.critters[#self.critters + 1] = critter
    end
    self.popped, self.launched = 0, 0
    self.pull = m.vec2(-110, 60)
    self:loadShot()

    self.world.onHit = function(a, b, contact)
        if contact.speed < Slingshot.popSpeed then
            return
        end
        for _, body in ipairs({a, b}) do
            if body and body.data and body.data.target then
                self.pops:fire(body.x, body.y)
                self.popped = self.popped + 1
                body:destroy()
            end
        end
    end
end

function Slingshot:loadShot()
    self.shot = self.world:createBody({type = 'kinematic', x = Slingshot.pouch[1] + self.pull.x, y = Slingshot.pouch[2] + self.pull.y})
    self.shot:addCircle(Slingshot.shotRadius, {category = Slingshot.shotCategory, density = 4, restitution = 0.2})
    Look.dress(self.shot, 'stone')
    self.reload = nil
end

function Slingshot:launch()
    if not self.shot or self.pull:length() < 20 then
        return
    end
    self.shot.type = 'dynamic'
    self.shot.velocity = self.pull * -Slingshot.power
    self.shots[#self.shots + 1] = self.shot
    if #self.shots > Slingshot.maxShots then
        table.remove(self.shots, 1):destroy()
    end
    self.shot, self.dragging, self.reload = nil, false, Slingshot.reloadTime
    self.launched = self.launched + 1
end

function Slingshot:setPull(x, y)
    self.pull = m.vec2(x, y):clampedLength(Slingshot.maxPull)
end

function Slingshot:exit()
    Slingshot.super.exit(self)
    input.clearVirtual()
end

function Slingshot:update(dt)
    Slingshot.super.update(self, dt)
    if input.pressed('reset') then
        self:build()
    end
    local pointer, pouch = self.pointer, Slingshot.pouch
    local dx, dy = pointer.worldX - pouch[1], pointer.worldY - pouch[2]
    if pointer.pressed and self.shot and not ui.usingPointer() and dx * dx + dy * dy < Slingshot.maxPull * Slingshot.maxPull then
        self.dragging = true
    end
    if self.dragging then
        self:setPull(dx, dy)
        if pointer.released then
            self:launch()
        end
    end
    local aimX, aimY = input.vector('aim')
    if aimX ~= 0 or aimY ~= 0 then
        self:setPull(self.pull.x + aimX * 260 * dt, self.pull.y + aimY * 260 * dt)
    end
    if input.pressed('launch') then
        self:launch()
    end
    self.pops:update(dt)

    local velocity = self.pull * -Slingshot.power
    self.path, self.pathHit = self.world:predictPath(pouch[1] + self.pull.x, pouch[2] + self.pull.y, velocity.x, velocity.y, {steps = 150, radius = Slingshot.shotRadius, mask = Slingshot.worldCategory | Slingshot.targetCategory})
    local aim = self.pathHit and (self.pathHit.body.data and self.pathHit.body.data.target and 'a critter' or 'the towers') or 'nothing'
    self:status(string.format('%d of %d critters popped with %d shots. The next shot leaves at %d per second and its path meets %s.', self.popped, #self.critters, self.launched, math.floor(velocity:length() + 0.5), aim))
end

function Slingshot:fixedUpdate(step)
    if self.reload then
        self.reload = self.reload - step
        if self.reload <= 0 then
            self:loadShot()
        end
    end
    if self.shot then
        self.shot:setTransform(Slingshot.pouch[1] + self.pull.x, Slingshot.pouch[2] + self.pull.y, 0)
    end
    self:simulate(self.world, step)
end

-- A critter is a round green slime with eyes that look toward the slingshot.
function Slingshot:drawCritters()
    local unit = Look.pixel
    for _, critter in ipairs(self.critters) do
        if critter.valid then
            local x, y = critter.x, critter.y
            graphics2d.drawShape({x - 28, y - 26, 56, 52}, {kind = 'ellipse', color = '#FF7BC74D', borderWidth = 3.5 * unit, borderColor = Look.ink, layer = 2})
            graphics2d.drawShape({x - 20, y - 16, 22, 12}, {kind = 'ellipse', rotation = -0.5, color = '#FFC2EE96', layer = 2})
            for _, side in ipairs({-10, 6}) do
                graphics2d.drawCircle(x + side, y - 4, 7, '#FFFFFFFF', {layer = 2})
                graphics2d.drawCircle(x + side - 2, y - 4, 3.5, Look.ink, {layer = 2})
            end
        end
    end
end

-- The path draws behind the slingshot and starts past its fork, as a row of dots that fades toward its end.
function Slingshot:drawPath()
    local unit = Look.pixel
    local path = self.path
    for index = 6, #path, 3 do
        local fade = 1 - index / #path
        graphics2d.drawCircle(path[index].x, path[index].y, 5 * unit, {1, 1, 1, 0.35 + fade * 0.55}, {layer = 1})
    end
    if self.pathHit then
        graphics2d.drawRing(self.pathHit.x, self.pathHit.y, 16, 4 * unit, '#FFFFC93C', {layer = 1})
    end
end

function Slingshot:draw(area)
    Scenery.sky(self.camera, Slingshot.ground)
    Scenery.ground(Slingshot.ground)
    Look.drawAll(self.blocks, {layer = 1})
    self:drawCritters()
    Look.drawAll(self.shots, {layer = 2})
    if self.shot and self.showPath then
        self:drawPath()
    end

    -- The far band goes behind the shot and the near one in front of it, as the bands of a real slingshot wrap the pouch.
    local pouch = Slingshot.pouch
    Look.block(pouch[1], pouch[2] + 100, 24, 180, 'darkWood', 3)
    Look.drawShape(self.fork, 4)
    local unit = Look.pixel
    local bx, by = pouch[1], pouch[2] - 4
    if self.shot then
        bx, by = self.shot.x, self.shot.y
    end
    local far, near = Slingshot.prongs[2], Slingshot.prongs[1]
    Look.line(far[1], far[2], bx, by, 7 * unit, '#FF8A3A2E', 3)
    if self.shot then
        Look.draw(self.shot, {layer = 5})
    end
    Look.line(near[1], near[2], bx, by, 7 * unit, '#FFB0503E', 6)
    self.pops:draw()
end

return Slingshot
