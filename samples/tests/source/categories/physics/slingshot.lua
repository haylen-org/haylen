-- A slingshot pulled back with the pointer or aimed with the keys or a stick, whose shot follows the path that `world:predictPath` draws before the launch, at targets that pop when hit hard.
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local input = require('haylen.input')
local m = require('haylen.math')
local physics2d = require('haylen.physics2d')
local ui = require('haylen.ui')

local parts = require('categories.physics.parts')
local PhysicsTest = require('categories.physics.physics-test')

local Slingshot = haylen.class('Slingshot', PhysicsTest)

Slingshot.actions = {
    {name = 'aim', type = 'vector', up = {'key:w', 'key:up', 'button:dpadUp'}, down = {'key:s', 'key:down', 'button:dpadDown'}, left = {'key:a', 'key:left', 'button:dpadLeft'}, right = {'key:d', 'key:right', 'button:dpadRight'}, bindings = {'stick:left', 'virtualStick:aim'}},
    {name = 'launch', type = 'button', bindings = {'key:space', 'button:south', 'virtual:launch'}},
}

local kWorld, kTarget, kShot = 1, 2, 4
local kPouch = {-560, 200}
local kProngs = {{-588, 196}, {-532, 196}}
local kMaxPull = 160
local kPower = 7.5
local kShotRadius = 20
local kReload = 1
local kMaxShots = 6
local kPopSpeed = 260
local kPuffLife = 0.5

function Slingshot:enter()
    self:frame{
        hint = 'Drag the shot back from the pouch and let go, or aim with the arrows, WASD or the left stick and launch with Space or the south button. The dotted line is the predicted path. R or the X button builds the targets again.',
        controls = {
            ui.button{id = 'launch', text = 'Launch', onClick = function() self:launch() end},
            ui.checkbox{id = 'path', text = 'Show the predicted path', checked = true, onChange = function(event) self.showPath = event.checked end},
            ui.button{id = 'reset', text = 'Build the targets again', onClick = function() self:build() end},
        },
        play = true,
        actions = Slingshot.actions,
        overlay = {
            ui.touchStick{action = 'aim', radius = 110, mode = 'floating', touchOnly = true, anchor = 'bottomLeft', margin = {0, 0, 110, 40}, width = 360, height = 300},
            ui.touchButton{action = 'launch', text = 'Launch', size = 150, touchOnly = true, anchor = 'bottomRight', margin = {0, 540, 110, 0}},
        },
    }
    self.showPath = true
    self:build()
end

function Slingshot:build()
    self.world = physics2d.newWorld()
    local ground = self.world:createBody({type = 'static', x = 0, y = 410})
    parts.box(ground, 1600, 40, {category = kWorld})
    local frame = self.world:createBody({type = 'static', x = kPouch[1], y = kPouch[2]})
    parts.box(frame, 22, 190, {offsetY = 95, mask = 0})
    parts.polygon(frame, {{-40, -10}, {-24, -10}, {-6, 20}, {6, 20}, {24, -10}, {40, -10}, {12, 40}, {-12, 40}}, {mask = 0})
    parts.paint(frame, '#FF8D6E63')
    self.statics = {ground, frame}

    self.blocks, self.targets, self.puffs, self.shots = {}, {}, {}, {}
    for _, spec in ipairs({{250, 330, 20, 120}, {390, 330, 20, 120}, {320, 260, 180, 20}, {270, 200, 20, 100}, {370, 200, 20, 100}, {320, 140, 140, 20}, {560, 360, 60, 60}, {640, 360, 60, 60}, {600, 300, 60, 60}}) do
        local block = self.world:createBody({x = spec[1], y = spec[2]})
        parts.box(block, spec[3], spec[4], {category = kWorld, density = 0.8})
        parts.paint(block, '#FFBCAAA4')
        self.blocks[#self.blocks + 1] = block
    end
    for _, spot in ipairs({{320, 363}, {320, 223}, {600, 243}}) do
        local target = self.world:createBody({x = spot[1], y = spot[2]})
        parts.circle(target, 26, {category = kTarget, density = 0.5})
        parts.paint(target, '#FF81C784')
        target.data.target = true
        self.targets[#self.targets + 1] = target
    end
    self.popped, self.launched = 0, 0
    self.pull = m.vec2(-110, 60)
    self:loadShot()

    self.world.onHit = function(a, b, contact)
        if contact.speed < kPopSpeed then
            return
        end
        for _, body in ipairs({a, b}) do
            if body and body.data.target then
                self.puffs[#self.puffs + 1] = {x = body.x, y = body.y, life = kPuffLife}
                self.popped = self.popped + 1
                body:destroy()
            end
        end
    end
end

function Slingshot:loadShot()
    self.shot = self.world:createBody({type = 'kinematic', x = kPouch[1] + self.pull.x, y = kPouch[2] + self.pull.y})
    parts.circle(self.shot, kShotRadius, {category = kShot, density = 4, restitution = 0.2})
    parts.paint(self.shot, '#FFE57373')
    self.reload = nil
end

function Slingshot:launch()
    if not self.shot or self.pull:length() < 20 then
        return
    end
    self.shot.type = 'dynamic'
    self.shot.velocity = self.pull * -kPower
    self.shots[#self.shots + 1] = self.shot
    if #self.shots > kMaxShots then
        table.remove(self.shots, 1):destroy()
    end
    self.shot, self.dragging, self.reload = nil, false, kReload
    self.launched = self.launched + 1
end

function Slingshot:setPull(x, y)
    self.pull = m.vec2(x, y):clampedLength(kMaxPull)
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
    local pointer = self.pointer
    local dx, dy = pointer.worldX - kPouch[1], pointer.worldY - kPouch[2]
    if pointer.pressed and self.shot and not ui.usingPointer() and dx * dx + dy * dy < kMaxPull * kMaxPull then
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
    for index = #self.puffs, 1, -1 do
        self.puffs[index].life = self.puffs[index].life - dt
        if self.puffs[index].life <= 0 then
            table.remove(self.puffs, index)
        end
    end

    local velocity = self.pull * -kPower
    self.path, self.pathHit = self.world:predictPath(kPouch[1] + self.pull.x, kPouch[2] + self.pull.y, velocity.x, velocity.y, {steps = 150, radius = kShotRadius, mask = kWorld | kTarget})
    local hit = self.pathHit and string.format('hits at %.0f, %.0f', self.pathHit.x, self.pathHit.y) or 'hits nothing'
    self:status(string.format('Shots %d, targets popped %d of %d, launch speed %.0f, predicted path of %d points %s, step %.2f ms', self.launched, self.popped, #self.targets, velocity:length(), #self.path, hit, self:stepTime()))
end

function Slingshot:fixedUpdate(step)
    if self.reload then
        self.reload = self.reload - step
        if self.reload <= 0 then
            self:loadShot()
        end
    end
    if self.shot then
        self.shot:setTransform(kPouch[1] + self.pull.x, kPouch[2] + self.pull.y, 0)
    end
    self:simulate(self.world, step)
end

function Slingshot:draw(area)
    parts.drawAll(self.statics)
    parts.drawAll(self.blocks)
    parts.drawAll(self.targets, {layer = 1})
    parts.drawAll(self.shots, {layer = 1})
    local band = self.shot and {self.shot.x, self.shot.y} or {kPouch[1], kPouch[2] - 4}
    for _, prong in ipairs(kProngs) do
        graphics2d.drawLine(prong[1], prong[2], band[1], band[2], 6, '#FF5D4037', {layer = 2})
    end
    if self.shot then
        parts.draw(self.shot, {layer = 3})
        if self.showPath then
            for index = 3, #self.path, 3 do
                graphics2d.drawCircle(self.path[index].x, self.path[index].y, 4, '#CCFFFFFF', {layer = 4})
            end
            if self.pathHit then
                graphics2d.drawRing(self.pathHit.x, self.pathHit.y, 14, 3, '#FFFFD54F', {layer = 4})
            end
        end
    end
    for _, puff in ipairs(self.puffs) do
        local age = 1 - puff.life / kPuffLife
        graphics2d.drawRing(puff.x, puff.y, 26 + age * 50, 4, m.color(0.5, 0.9, 0.5, 1 - age), {layer = 5})
    end
end

return Slingshot
