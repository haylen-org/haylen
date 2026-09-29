-- One run of the game: the island, the fire, the player, the trees and the waves of enemies that come every night.
local audio = require('haylen.audio')
local graphics2d = require('haylen.graphics2d')
local m = require('haylen.math')

local arrow = require('entities.arrow')
local campfire = require('systems.campfire')
local config = require('config')
local dayNight = require('systems.day-night')
local effects = require('systems.effects')
local enemy = require('entities.enemy')
local island = require('systems.island')
local player = require('entities.player')
local sound = require('systems.sound')
local tree = require('entities.tree')
local wood = require('entities.wood')

local game = {}
game.__index = game

-- The light that follows the player.
local lantern = m.color('#FFBFD8FF')

local function distanceSquared(ax, ay, bx, by)
    return (bx - ax) ^ 2 + (by - ay) ^ 2
end

function game.new(class)
    local self = setmetatable({class = class, enemies = {}, trees = {}, woods = {}, arrows = {}, kills = 0, pending = {}, spawnTime = 0}, game)
    self.island = island.new()
    self.effects = effects.new()
    self.campfire = campfire.new(self.island.world, self.island.fire.x, self.island.fire.y)
    self.player = player.new(self, class, self.island.start.x, self.island.start.y)

    self.camera = graphics2d.newCamera()
    self.camera.limits = self.island.bounds
    self.camera.positionSmoothing = true
    self.camera.positionSmoothingSpeed = 6
    self.camera:snapTo(self.island.start.x, self.island.start.y)

    self.cycle = dayNight.new(config.cycle, {day = '#FFFFFFFF', dawn = '#FFFFD9BF', dusk = '#FFF2A57E', night = '#FF26306E'}, config.cycle.dawn)
    self.cycle.onPhase = function(phase)
        self:phaseChanged(phase)
    end
    self.cycle.onDay = function(day)
        self:newDay(day)
    end

    self:plantTrees()
    sound.music('day')
    return self
end

function game:plantTrees()
    for _, spot in ipairs(self.island:treeSpots()) do
        self.trees[#self.trees + 1] = tree.new(self.island.world, spot.x, spot.y, spot.variant)
    end
end

function game:night()
    return self.cycle.phase == 'night' or self.cycle.phase == 'dusk'
end

-- Hands a localization key to the screen, which shows it as a notice.
function game:notify(key)
    if self.onNotice then
        self.onNotice(key)
    end
end

-- Dusk warns the player, the raiders land when night comes, and dawn burns the ones still standing.
function game:phaseChanged(phase)
    if phase == 'dusk' then
        sound.play('dusk')
        sound.music('night', 3)
        self:notify('hud.nightFalls')
    elseif phase == 'night' then
        self:queueWave(self.cycle.day)
    elseif phase == 'dawn' then
        sound.play('dawn')
        sound.music('day', 3)
        self:notify('hud.morning')
        self.pending = {}
        for _, raider in ipairs(self.enemies) do
            raider:ignite()
        end
    end
end

function game:newDay(day)
    self.campfire:newDay()
    for _, standing in ipairs(self.trees) do
        standing:regrow(day)
    end
end

-- Each night brings more raiders, with archers from the second night, lancers from the third and black units from the fourth.
function game:queueWave(night)
    local settings = config.enemies
    local count = settings.firstNight + (night - 1) * settings.perNight
    local kinds = {'warrior'}
    if night >= 2 then
        kinds[#kinds + 1] = 'archer'
    end
    if night >= 3 then
        kinds[#kinds + 1] = 'lancer'
    end
    for index = 1, count do
        local color = night >= settings.blackFromNight and index % 3 == 0 and 'black' or 'red'
        self.pending[#self.pending + 1] = {kind = kinds[math.random(#kinds)], color = color}
    end
    self.spawnTime = 0
end

function game:spawnPending(dt)
    self.spawnTime = self.spawnTime - dt
    if #self.pending == 0 or self.spawnTime > 0 then
        return
    end
    self.spawnTime = config.enemies.spawnInterval * (0.6 + math.random() * 0.8)
    local next = table.remove(self.pending, 1)
    local spawn = self.island.spawns[math.random(#self.island.spawns)]
    self.enemies[#self.enemies + 1] = enemy.new(self, next.kind, next.color, spawn.x, spawn.y)
    self.effects:animate('splash', spawn.x, spawn.y, 0.8)
end

function game:closestEnemy(x, y, reach)
    local best, bestDistance = nil, reach * reach
    for _, target in ipairs(self.enemies) do
        if target.alive and not target.burnTime then
            local ex, ey = target:position()
            local d = distanceSquared(x, y, ex, ey)
            if d < bestDistance then
                best, bestDistance = target, d
            end
        end
    end
    return best
end

function game:treeInReach(x, y, reach, facing)
    local best, bestDistance = nil, reach * reach
    for _, standing in ipairs(self.trees) do
        if standing:standing() then
            local d = distanceSquared(x, y, standing.x, standing.y)
            local inFront = (standing.x - x) * facing > -30
            if d < bestDistance and inFront then
                best, bestDistance = standing, d
            end
        end
    end
    return best
end

function game:chop(target, strength)
    sound.play('chop', target.x, target.y)
    self.effects:burst('chips', target.x + math.random(-20, 20), target.y - 50, 8)
    if target:chop(strength) then
        target:fell(self.cycle.day)
        sound.play('treeFall', target.x, target.y)
        self.effects:burst('chips', target.x, target.y - 80, 24)
        self.camera:addTrauma(0.2)
        for _ = 1, math.random(config.trees.woodMin, config.trees.woodMax) do
            local angle = math.random() * math.pi * 2
            local distance = 50 + math.random() * 50
            self.woods[#self.woods + 1] = wood.new(target.x, target.y - 30, target.x + math.cos(angle) * distance, target.y + math.sin(angle) * distance * 0.6 + 20)
        end
    end
end

-- Damages every enemy in reach, in front of the attacker when a direction is given, and pushes it away. A move that lasts several frames passes the set of enemies it already hit, so each takes the blow once.
function game:strike(x, y, reach, direction, damage, push, hit)
    for _, target in ipairs(self.enemies) do
        if target.alive and not target.burnTime and not (hit and hit[target]) then
            local ex, ey = target:position()
            local dx, dy = ex - x, ey - y
            local inReach = dx * dx + dy * dy < reach * reach
            local inFront = not direction or dx * direction.x + dy * direction.y > -20
            if inReach and inFront then
                if hit then
                    hit[target] = true
                end
                self:damageEnemy(target, damage, x, y, push)
            end
        end
    end
end

function game:damageEnemy(target, amount, fromX, fromY, push)
    local x, y = target:position()
    target:push(fromX, fromY, push)
    self.effects:burst('sparks', x, y - 50, 6)
    self.effects:number(x, y - 90, tostring(math.floor(amount + 0.5)))
    sound.play('hit', x, y)
    if target:hurt(amount) then
        self:killEnemy(target, false)
    else
        sound.play('enemyHurt', x, y)
    end
end

function game:killEnemy(target, burned)
    if not target.alive then
        return
    end
    local x, y = target:position()
    target:destroy()
    self.effects:animate(burned and 'bigExplosion' or 'explosion', x, y - 40, 1)
    sound.play(burned and 'explosion' or 'enemyDie', x, y)
    if not burned then
        self.kills = self.kills + 1
    end
end

function game:strikePlayer(attacker, damage)
    if not self.player.alive then
        return
    end
    local x, y = attacker:position()
    local px, py = self.player:position()
    if distanceSquared(x, y, px, py) < (attacker.kind.range + 30) ^ 2 and not self.campfire:contains(px, py) then
        self:hurtPlayer(damage, x, y)
    end
end

function game:hurtPlayer(damage, fromX, fromY)
    if self.player:damage(damage, fromX, fromY) then
        local x, y = self.player:position()
        self.player:destroy()
        self.effects:animate('bigExplosion', x, y - 40, 1.2)
        self.over = true
    end
end

function game:shoot(owner, x, y, dx, dy, damage, pierce)
    self.arrows[#self.arrows + 1] = arrow.new(owner, x, y, dx, dy, damage, pierce)
    sound.play('arrow', x, y)
end

function game:nearFire(x, y, reach)
    return distanceSquared(x, y, self.campfire.x, self.campfire.y) < reach * reach
end

function game:collectWood(collector)
    local x, y = collector:position()
    for index = #self.woods, 1, -1 do
        local piece = self.woods[index]
        if piece.landed and collector.wood < collector.class.capacity and distanceSquared(x, y, piece.x, piece.y) < 60 * 60 then
            table.remove(self.woods, index)
            collector.wood = collector.wood + 1
            sound.play('pickup', x, y)
        end
    end
end

function game:deliverWood(carrier)
    local taken = self.campfire:feed(carrier.wood)
    if taken > 0 then
        carrier.wood = carrier.wood - taken
        sound.play('feed', self.campfire.x, self.campfire.y)
        self.effects:burst('sparks', self.campfire.x, self.campfire.y - 40, 12)
        self.effects:number(self.campfire.x, self.campfire.y - 110, '+' .. taken * config.fire.woodFuel, '#FFFFD27A')
    end
end

-- Moves enemies that a growing circle swallowed back out to its edge.
function game:clearCircle()
    local fire = self.campfire
    for _, target in ipairs(self.enemies) do
        local x, y = target:position()
        if target.alive and fire:contains(x, y, 30) then
            local dx, dy = x - fire.x, y - fire.y
            local length = math.max(1, math.sqrt(dx * dx + dy * dy))
            target.body:setTransform(fire.x + dx / length * (fire.radius + 40), fire.y + dy / length * (fire.radius + 40), 0)
            target:push(fire.x, fire.y, 500)
        end
    end
end

function game:update(dt)
    local wasLit = self.campfire:lit()
    self.cycle:update(dt)
    self.island:update(dt)
    if self.campfire:update(dt, self:night()) then
        self:clearCircle()
    end
    if wasLit and not self.campfire:lit() then
        self:notify('hud.fireOut')
    end
    self:spawnPending(dt)

    if self.player.alive then
        self.player:update(dt)
    end

    local positions = {}
    for _, target in ipairs(self.enemies) do
        if target.alive then
            positions[#positions + 1] = {target:position()}
        end
    end
    for _, target in ipairs(self.enemies) do
        if target.alive then
            target:update(dt, positions)
        end
    end
    for index = #self.arrows, 1, -1 do
        local flying = self.arrows[index]
        flying:update(dt, self)
        if not flying.alive then
            table.remove(self.arrows, index)
        end
    end

    -- Enemies fall to blows, fire and arrows, and their bodies are gone, so they leave the list before anything reads them again.
    for index = #self.enemies, 1, -1 do
        if not self.enemies[index].alive then
            table.remove(self.enemies, index)
        end
    end
    for _, standing in ipairs(self.trees) do
        standing:update(dt)
    end
    for _, piece in ipairs(self.woods) do
        piece:update(dt)
    end

    self.island.world:step(dt)
    self.effects:update(dt)

    if self.player.alive then
        local x, y = self.player:position()
        self.camera:follow(x, y, dt)
        audio.setListener(x, y)
    end
    self.camera:update(dt)
end

-- Draws the island through the camera, washed out and darker when dimmed behind a menu.
function game:draw(dimmed)
    local postProcess = {vignetteStrength = 0.3}
    if dimmed then
        postProcess.saturation = 0.3
        postProcess.brightness = 0.6
    end
    graphics2d.beginWorld(self.camera, {sort = 'depth', ambientLight = self.cycle.ambient, postProcess = postProcess})
    self.island:drawGround(self.camera)
    self.campfire:draw()
    for _, standing in ipairs(self.trees) do
        standing:draw()
    end
    for _, piece in ipairs(self.woods) do
        piece:draw()
    end
    for _, target in ipairs(self.enemies) do
        target:draw()
    end
    if self.player.alive then
        self.player:draw()
    end
    for _, flying in ipairs(self.arrows) do
        flying:draw()
    end
    self.effects:draw()
    self.island:drawClouds(self.camera)

    self.campfire:light(self.cycle)
    if self.player.alive then
        local x, y = self.player:position()
        graphics2d.drawLight({x = x, y = y - 40, radius = 260, color = self.cycle:fill(lantern), intensity = 0.7})
    end
end

function game:destroy()
    self.campfire:destroy()
end

return game
