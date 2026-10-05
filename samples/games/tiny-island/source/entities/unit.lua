-- What the player, the raiders and the sheep share: a round body at the feet, an animated sprite with a shadow, health, a hit flash and knockback.
local graphics2d = require('haylen.graphics2d')

local art = require('systems.art')
local config = require('config')

local unit = {}
unit.__index = unit

-- Creates the body and the sprite of a unit. The animator gives the sprite its frame and the pivot at the feet every time it draws.
function unit.setup(self, world, character, x, y, radius, category, mask)
    self.body = world:createBody({type = 'dynamic', x = x, y = y, fixedRotation = true, linearDamping = 8})
    self.body:addCircle(radius, {category = category, mask = mask, friction = 0})
    self.radius = radius
    self.animator, self.art = art.newAnimator(character)
    self.sprite = graphics2d.newSprite(self.art.atlas.texture, {layer = config.layer.entities, scaleX = self.art.scale, scaleY = self.art.scale})
    self.shadow = art.effects()
    self.shadowSource = self.shadow:source('shadow')
    self.facing = 1
    self.flash = 0
    self.knockback = {x = 0, y = 0}
    self.alive = true
    self.x, self.y = x, y
end

function unit.position(self)
    if self.body then
        return self.body.x, self.body.y
    end
    return self.x, self.y
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
    local x, y = self:position()
    local dx, dy = x - fromX, y - fromY
    local length = math.max(1, math.sqrt(dx * dx + dy * dy))
    self.knockback.x = dx / length * strength
    self.knockback.y = dy / length * strength
end

-- Takes damage and returns `true` when it was the last of the health.
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

-- Whether a one-shot clip whose name starts with `prefix` is still playing.
function unit.busy(self, prefix)
    local current = self.animator.current
    return current ~= nil and current:sub(1, #prefix) == prefix and not self.animator.finished
end

function unit.animate(self, dt)
    self.animator:update(dt)
    self.flash = math.max(0, self.flash - dt * 5)
end

-- Leaves the physics world and keeps the sprite where the unit fell, for its last animation.
function unit.destroy(self)
    self.x, self.y = self:position()
    self.alive = false
    self.body:destroy()
    self.body = nil
end

function unit.draw(self, alpha)
    local x, y = self:position()
    local opacity = alpha or 1
    local width = self.radius * 3.2
    graphics2d.draw(self.shadow.texture, x, y + 2, {source = self.shadowSource, width = width, height = width * 0.38, color = string.format('#%02XFFFFFF', math.floor(110 * opacity)), layer = config.layer.entities, depth = y - 1})
    self.animator:apply(self.sprite)
    self.sprite.x = x
    self.sprite.y = y
    self.sprite.depth = y
    self.sprite.flipHorizontal = self.facing < 0
    self.sprite.color = string.format('#%02XFFFFFF', math.floor(255 * opacity))
    self.sprite.flash = string.format('#%02X%s', math.floor(self.flash * 230), self.flashColor or 'FFFFFF')
    self.sprite:draw()
end

return unit
