-- An arrow in flight. Player arrows can pass through enemies, and enemy arrows burn up when they fly into the light of the fire.
local graphics2d = require('haylen.graphics2d')

local art = require('systems.art')
local config = require('config')

local arrow = {}
arrow.__index = arrow

local speed = 950
local reach = 760

function arrow.new(owner, x, y, dx, dy, damage, pierce)
    local self = setmetatable({owner = owner, x = x, y = y, dx = dx, dy = dy, damage = damage, pierce = pierce, traveled = 0, hit = {}, alive = true}, arrow)
    self.texture = art.texture('units/' .. (owner.color or 'blue') .. '/archer/arrow.png')
    return self
end

function arrow:update(dt, game)
    local step = speed * dt
    self.x = self.x + self.dx * step
    self.y = self.y + self.dy * step
    self.traveled = self.traveled + step
    if self.traveled > reach then
        self.alive = false
        return
    end

    if self.owner == game.player then
        -- An enemy that another arrow killed this frame is still listed, but its body is already gone.
        for _, target in ipairs(game.enemies) do
            if target.alive and not self.hit[target] then
                local ex, ey = target:position()
                if (ex - self.x) ^ 2 + (ey - 40 - self.y) ^ 2 < 50 * 50 then
                    self.hit[target] = true
                    game:damageEnemy(target, self.damage, self.x - self.dx * 50, self.y - self.dy * 50, 220)
                    self.pierce = self.pierce - 1
                    if self.pierce <= 0 then
                        self.alive = false
                        return
                    end
                end
            end
        end
    elseif game.campfire:contains(self.x, self.y) then
        game.effects:burst('sparks', self.x, self.y, 6)
        self.alive = false
    elseif game.player.alive then
        local px, py = game.player:position()
        if (px - self.x) ^ 2 + (py - 40 - self.y) ^ 2 < 40 * 40 then
            game:hurtPlayer(self.damage, self.x - self.dx * 50, self.y - self.dy * 50)
            self.alive = false
        end
    end
end

function arrow:draw()
    graphics2d.draw(self.texture, self.x, self.y, {pivotX = 0.5, pivotY = 0.5, rotation = math.atan(self.dy, self.dx), layer = config.layer.entities, depth = self.y + 40})
end

return arrow
