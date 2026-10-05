-- A sheep that wanders the island, grazes, runs from whoever hurts it and leaves meat when it falls.
local config = require('config')
local unit = require('entities.unit')

local sheep = setmetatable({}, {__index = unit})
sheep.__index = sheep

function sheep.new(game, x, y)
    local self = setmetatable({game = game, isSheep = true, state = 'graze', stateTime = math.random() * 3}, sheep)
    local category = config.category
    unit.setup(self, game.island.world, 'sheep', x, y, 24, category.sheep, category.world | category.player | category.tree | category.sheep)
    self.health = config.sheep.health
    self.maxHealth = self.health
    self.facing = math.random() < 0.5 and -1 or 1
    return self
end

-- Takes a blow, bleats with a puff of wool and runs away from where the blow came from.
function sheep:startle(fromX, fromY)
    self.state = 'flee'
    self.stateTime = 1.6
    self.fromX, self.fromY = fromX, fromY
    self:play('hurt', true)
end

-- Picks the next thing to do: graze for a while, or walk to a spot nearby that the island lets it reach.
function sheep:decide()
    local x, y = self:position()
    if math.random() < 0.45 then
        self.state = 'graze'
        self.stateTime = 2 + math.random() * 3
        return
    end
    for _ = 1, 6 do
        local angle = math.random() * math.pi * 2
        local reach = 80 + math.random() * 160
        local tx, ty = x + math.cos(angle) * reach, y + math.sin(angle) * reach
        if self.game.island:walkable(tx, ty) then
            self.state = 'walk'
            self.stateTime = 4
            self.targetX, self.targetY = tx, ty
            return
        end
    end
end

function sheep:update(dt)
    self.stateTime = self.stateTime - dt
    local x, y = self:position()
    local vx, vy = 0, 0
    local settings = config.sheep

    if self.state == 'flee' then
        local dx, dy = x - self.fromX, y - self.fromY
        local length = math.max(1, math.sqrt(dx * dx + dy * dy))
        vx, vy = dx / length * settings.fleeSpeed, dy / length * settings.fleeSpeed
    elseif self.state == 'walk' then
        local dx, dy = self.targetX - x, self.targetY - y
        local length = math.sqrt(dx * dx + dy * dy)
        if length < 12 then
            self.stateTime = 0
        else
            vx, vy = dx / length * settings.speed, dy / length * settings.speed
        end
    end
    if self.stateTime <= 0 then
        self:decide()
    end
    unit.move(self, vx, vy, dt)

    if not unit.busy(self, 'hurt') then
        local moving = vx * vx + vy * vy > 100
        self:play(moving and 'run' or 'idle')
        self.animator.speed = self.state == 'flee' and 1.6 or 1
    end
    self:animate(dt)
end

function sheep:fall()
    self:play('death', true)
    unit.destroy(self)
end

return sheep
