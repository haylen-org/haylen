-- A projectile in flight: the arrows of the archer and the fireballs of the mage, and the javelins and bombs of the raiders. Raider projectiles burn up in the light of the fire, which keeps the camp safe.
local graphics2d = require('haylen.graphics2d')
local m = require('haylen.math')

local art = require('systems.art')
local config = require('config')

local projectile = {}
projectile.__index = projectile

-- Arrows and javelins fly straight and hit what they touch, a fireball bursts on its first hit, and a bomb arcs to the point it was thrown at and bursts there.
local kinds = {
    arrow = {speed = 980, reach = 760, frame = 'arrow', radius = 46, push = 240},
    javelin = {speed = 760, reach = 640, frame = 'javelin', radius = 40, push = 300},
    fireball = {speed = 640, reach = 620, radius = 48, blast = 125, push = 440},
    bomb = {flight = 0.85, arc = 170, frame = 'bomb', blast = 110, push = 480},
}

local fireLight = m.color('#FFFF9A40')

-- Creates a projectile from `x`, `y` toward the direction `dx`, `dy`, or for a bomb toward the point `dx`, `dy`. The table `options` may give `pierce`, how many enemies an arrow passes through.
function projectile.new(game, owner, kind, x, y, dx, dy, damage, options)
    local spec = kinds[kind]
    local self = setmetatable({game = game, owner = owner, kind = kind, spec = spec, x = x, y = y, damage = damage, traveled = 0, time = 0, hit = {}, alive = true}, projectile)
    self.hostile = owner ~= game.player
    self.pierce = options and options.pierce or 1
    self.atlas = art.effects()
    if spec.frame then
        self.source = self.atlas:source(spec.frame)
    end
    if kind == 'fireball' then
        self.clip = art.effect('fireball', 14, 'loop')
    end
    if kind == 'bomb' then
        self.fromX, self.fromY, self.toX, self.toY = x, y, dx, dy
    else
        self.dx, self.dy = dx, dy
    end
    return self
end

function projectile:update(dt)
    self.time = self.time + dt
    if self.kind == 'bomb' then
        self:fly(dt)
        return
    end

    local step = self.spec.speed * dt
    self.x = self.x + self.dx * step
    self.y = self.y + self.dy * step
    self.traveled = self.traveled + step
    if self.kind == 'fireball' and math.random() < dt * 40 then
        self.game.effects:burst('embers', self.x - self.dx * 20, self.y - self.dy * 20, 1)
    end
    if self.traveled > self.spec.reach then
        self:finish()
        return
    end

    if self.hostile then
        self:hitPlayer()
    else
        self:hitTargets()
    end
end

-- A bomb follows an arc from the hand of the thrower to its target and bursts when it lands.
function projectile:fly(dt)
    local t = math.min(1, self.time / self.spec.flight)
    self.x = self.fromX + (self.toX - self.fromX) * t
    self.y = self.fromY + (self.toY - self.fromY) * t
    self.height = 4 * self.spec.arc * t * (1 - t)
    if t >= 1 then
        self:finish()
    end
end

-- Player projectiles hit raiders and sheep. An enemy that another projectile killed this frame is still listed, but it is no longer alive.
function projectile:hitTargets()
    for _, target in ipairs(self.game:targets()) do
        if not self.hit[target] then
            local tx, ty = target:position()
            if (tx - self.x) ^ 2 + (ty - 45 - self.y) ^ 2 < self.spec.radius ^ 2 then
                if self.kind == 'fireball' then
                    self:finish()
                    return
                end
                self.hit[target] = true
                self.game:hit(target, self.damage, self.x - self.dx * 50, self.y - self.dy * 50, self.spec.push)
                self.pierce = self.pierce - 1
                if self.pierce <= 0 then
                    self.alive = false
                    return
                end
            end
        end
    end
end

function projectile:hitPlayer()
    local game = self.game
    if game.campfire:contains(self.x, self.y) then
        game.effects:burst('sparks', self.x, self.y, 8)
        self.alive = false
        return
    end
    local player = game.player
    if player.alive then
        local px, py = player:position()
        if (px - self.x) ^ 2 + (py - 45 - self.y) ^ 2 < self.spec.radius ^ 2 then
            game:hurtPlayer(self.damage, self.x - self.dx * 50, self.y - self.dy * 50)
            self.alive = false
        end
    end
end

-- Ends the flight: fireballs and bombs burst, while arrows and javelins simply drop.
function projectile:finish()
    self.alive = false
    if self.spec.blast then
        self.game:explode(self.x, self.y, self.spec.blast, self.damage, self.spec.push, self.hostile)
    end
end

function projectile:draw()
    local layer = config.layer.entities
    if self.kind == 'bomb' then
        local t = math.min(1, self.time / self.spec.flight)
        graphics2d.drawRing(self.toX, self.toY, self.spec.blast * (0.35 + 0.65 * t), 4, string.format('#%02XFF5040', math.floor(90 + 140 * t)), {layer = config.layer.ring})
        graphics2d.draw(self.atlas.texture, self.x, self.y - self.height, {source = self.source, rotation = self.time * 9, layer = layer, depth = self.y + 60})
        return
    end
    local rotation = math.atan(self.dy, self.dx)
    if self.clip then
        graphics2d.draw(self.clip.texture, self.x, self.y, {source = self.clip:frame(self.clip:frameAt(self.time)), rotation = rotation, scaleX = 0.9, scaleY = 0.9, layer = config.layer.effects, depth = self.y + 60, unshaded = true})
        return
    end
    -- Arrows and javelins leave a short streak of air behind them.
    local trail = math.min(self.traveled, 90)
    graphics2d.drawLine(self.x - self.dx * trail, self.y - self.dy * trail, self.x - self.dx * 30, self.y - self.dy * 30, 5, '#70FFFFFF', {layer = layer, depth = self.y + 59})
    graphics2d.draw(self.atlas.texture, self.x, self.y, {source = self.source, rotation = rotation, layer = layer, depth = self.y + 60})
end

-- A fireball lights the ground it flies over.
function projectile:light(cycle)
    if self.kind == 'fireball' then
        graphics2d.drawLight({x = self.x, y = self.y + 30, radius = 220, color = cycle:fill(fireLight), intensity = 0.9})
    end
end

return projectile
