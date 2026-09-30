-- What the player and the enemies share: a round body at the feet, an animated sprite, health, a hit flash and knockback.
local graphics2d = require('haylen.graphics2d')

local art = require('systems.art')
local config = require('config')

local unit = {}
unit.__index = unit

local radius = 22

-- Creates the body and the sprite of a unit. The animator gives the sprite its frame and the pivot at the feet every time it draws.
function unit.setup(self, world, kind, color, x, y, category, mask)
    self.body = world:createBody({type = 'dynamic', x = x, y = y, fixedRotation = true, linearDamping = 8})
    self.body:addCircle(radius, {category = category, mask = mask, friction = 0})
    self.animator, self.art = art.newAnimator(kind, color)
    self.sprite = graphics2d.newSprite(self.art.clips.idle.texture, {layer = config.layer.entities})
    self.facing = 1
    self.flash = 0
    self.knockback = {x = 0, y = 0}
    self.alive = true
end

function unit.position(self)
    return self.body.x, self.body.y
end

-- Moves with a velocity in pixels per second, keeping any knockback on top of it while it fades.
function unit.move(self, vx, vy, dt)
    local fade = math.exp(-10 * dt)
    self.knockback.x = self.knockback.x * fade
    self.knockback.y = self.knockback.y * fade
    self.body.velocity = {vx + self.knockback.x, vy + self.knockback.y}
    if math.abs(vx) > 1 then
        self.facing = vx > 0 and 1 or -1
    end
end

function unit.push(self, fromX, fromY, strength)
    local x, y = self.body.x, self.body.y
    local dx, dy = x - fromX, y - fromY
    local length = math.max(1, math.sqrt(dx * dx + dy * dy))
    self.knockback.x = dx / length * strength
    self.knockback.y = dy / length * strength
end

function unit.hurt(self, amount)
    self.health = math.max(0, self.health - amount)
    self.flash = 1
    return self.health <= 0
end

-- Plays a looping clip unless it already plays, and restarts a one-shot clip every time.
function unit.play(self, clip, restart)
    if restart or self.animator.current ~= clip then
        self.animator:play(clip, restart)
    end
end

function unit.animate(self, dt)
    self.animator:update(dt)
    self.flash = math.max(0, self.flash - dt * 5)
end

function unit.draw(self)
    self.animator:apply(self.sprite)
    self.sprite.x = self.body.x
    self.sprite.y = self.body.y
    self.sprite.depth = self.body.y
    self.sprite.flipHorizontal = self.facing < 0
    self.sprite.flash = string.format('#%02X%s', math.floor(self.flash * 255), self.flashColor or 'FFFFFF')
    self.sprite:draw()
end

function unit.destroy(self)
    self.alive = false
    self.body:destroy()
end

return unit
