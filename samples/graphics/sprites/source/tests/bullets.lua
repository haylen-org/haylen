-- Pooled bullets: a bullet hell whose projectiles come from an object pool, so firing thousands of them creates no garbage, and draw in one batch with an additive blend.
local collections = require('haylen.collections')
local debugging = require('haylen.debug')
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local ui = require('haylen.ui')

local sample = require('sample')

local Bullets = haylen.class('Bullets', sample.Test)

local kPatterns = {{id = 'spiral', text = 'Spiral'}, {id = 'rings', text = 'Rings'}, {id = 'aimed', text = 'Aimed'}}
local kSources = {{0, 0, 16, 16}, {16, 0, 16, 16}, {32, 0, 16, 16}, {48, 0, 16, 16}}
local kCapacity = 5000
local kCode = [[
local bullets = collections.newPool({create = newBullet, reset = function(bullet, x, y, vx, vy) ... end, capacity = 5000, prewarm = 500})
bullets:acquire(x, y, vx, vy)  bullets:release(bullet)  -- recycled, never collected
bullets:each(move)  graphics2d.drawBatch(texture, sprites, {blend = 'additive'})]]

function Bullets:enter()
    self.texture = sample.texture('images/bullets.png')
    self.pattern, self.rate = 'spiral', 1
    self.angle, self.cooldown = 0, 0
    self.sprites = {}
    self.pool = collections.newPool({
        create = function()
            return {vx = 0, vy = 0, sprite = {x = 0, y = 0, width = 20, height = 20, rotation = 0, source = kSources[1]}}
        end,
        reset = function(bullet, x, y, angle, speed, kind)
            bullet.vx, bullet.vy = math.cos(angle) * speed, math.sin(angle) * speed
            local sprite = bullet.sprite
            sprite.x, sprite.y, sprite.rotation, sprite.source = x, y, angle, kSources[kind]
        end,
        capacity = kCapacity,
        prewarm = 500,
    })
    -- The walk over the pool is one function made once, so moving the bullets allocates nothing.
    self.move = function(bullet)
        local sprite = bullet.sprite
        sprite.x, sprite.y = sprite.x + bullet.vx * self.dt, sprite.y + bullet.vy * self.dt
        if sprite.x < -20 or sprite.y < -20 or sprite.x > self.area.width + 20 or sprite.y > self.area.height + 20 then
            self.pool:release(bullet)
        else
            self.count = self.count + 1
            self.sprites[self.count] = sprite
        end
    end
    self:frame({
        hint = 'In the aimed pattern the emitter shoots at the pointer.',
        code = kCode,
        controls = {
            ui.segmentedControl{id = 'pattern', items = kPatterns, selected = 'spiral', onChange = function(event) self.pattern = event.value end},
            ui.formField{label = 'Fire rate', ui.slider{id = 'rate', min = 0.5, max = 4, value = 1, step = 0.5, showValue = true, onChange = function(event) self.rate = event.value end}},
            ui.button{id = 'clear', text = 'Release every bullet', onClick = function() self.pool:releaseAll() end},
        },
        focus = 'pattern',
    })
end

function Bullets:fire(x, y, angle, speed, kind)
    self.pool:acquire(x, y, angle, speed, kind)
end

-- Fires one volley of the current pattern from the emitter.
function Bullets:volley(x, y)
    if self.pattern == 'spiral' then
        self.angle = self.angle + 0.21
        for arm = 0, 4 do
            self:fire(x, y, self.angle + arm * math.pi * 2 / 5, 260, 1)
        end
        return 0.05
    elseif self.pattern == 'rings' then
        self.angle = self.angle + 0.13
        for index = 0, 35 do
            self:fire(x, y, self.angle + index * math.pi * 2 / 36, 200, index % 2 == 0 and 2 or 3)
        end
        return 0.3
    end
    local targetX, targetY = self:pointer()
    local aim = math.atan(targetY - y, targetX - x)
    for index = -3, 3 do
        self:fire(x, y, aim + index * 0.09, 420, 4)
    end
    return 0.12
end

function Bullets:update(dt)
    Bullets.super.update(self, dt)
    if not self.area then
        return
    end
    local x, y = self.area.width / 2, self.area.height * 0.4
    self.cooldown = self.cooldown - dt * self.rate
    while self.cooldown <= 0 do
        self.cooldown = self.cooldown + self:volley(x, y)
    end

    self.dt, self.count = dt, 0
    self.pool:each(self.move)
    for index = self.count + 1, #self.sprites do
        self.sprites[index] = nil
    end
    local frame = debugging.frame()
    self:status(string.format('%d active   %d idle   capacity %d   %.0f FPS   %.2f ms', self.pool.active, self.pool.idle, self.pool.capacity, frame.fps, frame.milliseconds))
end

function Bullets:draw(area)
    local x, y = area.width / 2, area.height * 0.4
    graphics2d.drawCircle(x, y, 26, '#FF3A4058')
    graphics2d.drawRing(x, y, 30, 4, sample.warm)
    graphics2d.drawBatch(self.texture, self.sprites, {layer = 1, blend = 'additive'})
end

return Bullets
