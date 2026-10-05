-- One run of the game: the island, the fire, the player, the trees, the sheep and the waves of raiders that come every night.
local audio = require('haylen.audio')
local graphics2d = require('haylen.graphics2d')
local m = require('haylen.math')

local campfire = require('systems.campfire')
local config = require('config')
local dayNight = require('systems.day-night')
local effects = require('systems.effects')
local enemies = require('data.enemies')
local enemy = require('entities.enemy')
local island = require('systems.island')
local pickup = require('entities.pickup')
local player = require('entities.player')
local projectile = require('entities.projectile')
local sheep = require('entities.sheep')
local sound = require('systems.sound')
local tree = require('entities.tree')

local game = {}
game.__index = game

-- The light that follows the player, and the flash of a burst of fire.
local lantern = m.color('#FFBFD8FF')
local blaze = m.color('#FFFFB060')

-- A fallen raider or sheep stays on the ground for its last animation and this many seconds more while it fades.
local fadeTime = 0.8

local function distanceSquared(ax, ay, bx, by)
    return (bx - ax) ^ 2 + (by - ay) ^ 2
end

function game.new(class)
    local self = setmetatable({class = class, enemies = {}, herd = {}, fallen = {}, trees = {}, pickups = {}, projectiles = {}, flashes = {}, kills = 0, pending = {}, spawnTime = 0}, game)
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
    self:tendHerd(1)
    sound.music('day')
    return self
end

function game:plantTrees()
    for _, spot in ipairs(self.island:treeSpots()) do
        self.trees[#self.trees + 1] = tree.new(self.island.world, spot.x, spot.y, spot.variant)
    end
end

-- Brings the herd back to its size in the wild parts of the island, away from the fire and the player.
function game:tendHerd(day)
    local living = 0
    for _, animal in ipairs(self.herd) do
        if animal.alive then
            living = living + 1
        end
    end
    local px, py = self.player:position()
    local spots = self.island:scatter(m.random(day * 31), config.sheep.spacing, function(point)
        return distanceSquared(point.x, point.y, self.campfire.x, self.campfire.y) > 500 * 500 and distanceSquared(point.x, point.y, px, py) > 400 * 400
    end)
    for _ = 1, math.min(#spots, config.sheep.herd - living) do
        local spot = table.remove(spots, math.random(#spots))
        self.herd[#self.herd + 1] = sheep.new(self, spot.x, spot.y)
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
    self:tendHerd(day)
end

-- Each night brings more raiders, picked by weight among the kinds that already joined: imps and brutes from the first night, throwers from the second and bombers from the third.
function game:queueWave(night)
    local settings = config.enemies
    local count = settings.firstNight + (night - 1) * settings.perNight
    local kinds, total = {}, 0
    for name, kind in pairs(enemies) do
        if night >= kind.firstNight then
            kinds[#kinds + 1] = name
            total = total + kind.weight
        end
    end
    table.sort(kinds)
    for _ = 1, count do
        local roll = math.random() * total
        for _, name in ipairs(kinds) do
            roll = roll - enemies[name].weight
            if roll <= 0 then
                self.pending[#self.pending + 1] = name
                break
            end
        end
    end
    self.spawnTime = 0
end

-- Imps land in pairs, the others alone.
function game:spawnPending(dt)
    self.spawnTime = self.spawnTime - dt
    if #self.pending == 0 or self.spawnTime > 0 then
        return
    end
    self.spawnTime = config.enemies.spawnInterval * (0.6 + math.random() * 0.8)
    local kind = table.remove(self.pending, 1)
    local spawn = self.island.spawns[math.random(#self.island.spawns)]
    local group = kind == 'imp' and 2 or 1
    for index = 1, group do
        local x, y = spawn.x + (index - 1) * 50, spawn.y + (index - 1) * 30
        self.enemies[#self.enemies + 1] = enemy.new(self, kind, self.cycle.day, x, y)
    end
    self.effects:animate('splash', spawn.x, spawn.y - 20, {scale = 1.1})
end

-- Lists the living raiders and sheep that player attacks can hit.
function game:targets()
    local list = {}
    for _, target in ipairs(self.enemies) do
        if target.alive and not target.burnTime then
            list[#list + 1] = target
        end
    end
    for _, animal in ipairs(self.herd) do
        if animal.alive then
            list[#list + 1] = animal
        end
    end
    return list
end

local function closest(list, x, y, reach, eligible)
    local best, bestDistance = nil, reach * reach
    for _, target in ipairs(list) do
        if target.alive and eligible(target) then
            local tx, ty = target:position()
            local d = distanceSquared(x, y, tx, ty)
            if d < bestDistance then
                best, bestDistance = target, d
            end
        end
    end
    return best
end

function game:closestEnemy(x, y, reach)
    return closest(self.enemies, x, y, reach, function(target)
        return not target.burnTime
    end)
end

function game:closestSheep(x, y, reach)
    return closest(self.herd, x, y, reach, function()
        return true
    end)
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

-- Drops pieces of `kind` around a point, each hopping to a spot of its own.
function game:scatterPickups(kind, x, y, count)
    for _ = 1, count do
        local angle = math.random() * math.pi * 2
        local distance = 50 + math.random() * 50
        self.pickups[#self.pickups + 1] = pickup.new(kind, x, y - 30, x + math.cos(angle) * distance, y + math.sin(angle) * distance * 0.6 + 20)
    end
end

function game:chop(target, strength)
    sound.play('chop', target.x, target.y)
    self.effects:burst('chips', target.x + math.random(-20, 20), target.y - 50, 10)
    if target:chop(strength) then
        target:fell(self.cycle.day)
        sound.play('treeFall', target.x, target.y)
        self.effects:burst('chips', target.x, target.y - 80, 26)
        self.effects:burst('dust', target.x, target.y, 10)
        self.camera:addTrauma(0.2)
        self:scatterPickups('wood', target.x, target.y, math.random(config.trees.woodMin, config.trees.woodMax))
    end
end

-- Damages every raider and sheep in an area and pushes it away. The table `shape` gives the `reach`, and with a `direction` only what is in front counts, along a band of half the given `width` when there is one. A move that lasts several frames passes the set of targets it already hit, so each takes the blow once.
function game:strike(x, y, shape, damage, push, hit)
    local reach = shape.reach
    local direction = shape.direction
    for _, target in ipairs(self:targets()) do
        if not (hit and hit[target]) then
            local tx, ty = target:position()
            local dx, dy = tx - x, ty - y
            local inside
            if direction and shape.width then
                local along = dx * direction.x + dy * direction.y
                local across = math.abs(dx * direction.y - dy * direction.x)
                inside = along > -20 and along < reach and across < shape.width
            else
                inside = dx * dx + dy * dy < reach * reach and (not direction or dx * direction.x + dy * direction.y > -20)
            end
            if inside then
                if hit then
                    hit[target] = true
                end
                self:hit(target, damage, x, y, push)
            end
        end
    end
end

-- Lands a blow on a raider or a sheep, with sparks, a number and a sound, and fells it when its health runs out.
function game:hit(target, amount, fromX, fromY, push)
    local x, y = target:position()
    target:push(fromX, fromY, push)
    self.effects:animate('hit', x, y - 50, {scale = 0.7, glow = true, rotation = math.random() * 6.28})
    self.effects:number(x, y - 100, tostring(math.floor(amount + 0.5)))
    sound.play('hit', x, y)
    if target.isSheep then
        self.effects:burst('wool', x, y - 40, 10)
        target:startle(fromX, fromY)
        if target:hurt(amount) then
            self:fell(target)
            target:fall()
            self:scatterPickups('meat', x, y, config.sheep.meat)
        end
        return
    end

    self.effects:burst('sparks', x, y - 50, 8)
    if target:hurt(amount) then
        self:killEnemy(target, false)
    else
        sound.play('enemyHurt', x, y)
        if not target:busy('attack') then
            target:play('hurt', true)
        end
    end
end

-- Keeps a fallen raider or sheep on the ground until its last animation ends and it fades away.
function game:fell(target)
    self.fallen[#self.fallen + 1] = {unit = target, time = 0}
end

function game:killEnemy(target, burned)
    if not target.alive then
        return
    end
    local x, y = target:position()
    target:fall()
    if burned then
        self.effects:animate('explosion', x, y - 50, {scale = 0.9, glow = true})
        sound.play('explosion', x, y)
        return
    end
    self:fell(target)
    self.effects:burst('smoke', x, y - 30, 6)
    sound.play('enemyDie', x, y)
    self.kills = self.kills + 1
end

-- A raider blow that lands when the player is within `reach` and outside the light of the fire.
function game:strikePlayer(attacker, damage, reach)
    if not self.player.alive then
        return
    end
    local x, y = attacker:position()
    local px, py = self.player:position()
    if distanceSquared(x, y, px, py) < reach * reach and not self.campfire:contains(px, py) then
        self:hurtPlayer(damage, x, y)
    end
end

-- The smash of a brute hurts the player anywhere in a circle in front of it.
function game:smash(attacker, x, y, radius, damage)
    local px, py = self.player:position()
    if self.player.alive and distanceSquared(x, y, px, py) < radius * radius and not self.campfire:contains(px, py) then
        self:hurtPlayer(damage, attacker:position())
    end
end

function game:hurtPlayer(damage, fromX, fromY)
    if self.player.alive and self.player:damage(damage, fromX, fromY) then
        self:playerFell()
    end
end

function game:playerFell()
    self.player:fall()
    self.over = true
end

function game:shoot(kind, owner, x, y, dx, dy, damage, options)
    self.projectiles[#self.projectiles + 1] = projectile.new(self, owner, kind, x, y, dx, dy, damage, options)
    if kind == 'arrow' or kind == 'javelin' then
        sound.play('arrow', x, y)
    end
end

-- A burst of fire from a fireball or a bomb: it hurts the player when a raider threw it, and the raiders and sheep around it when the player did. The light of the fire puts out raider bombs.
function game:explode(x, y, radius, damage, push, hostile)
    if hostile and self.campfire:contains(x, y) then
        self.effects:burst('smoke', x, y, 8)
        return
    end
    self.effects:animate('explosion', x, y - 40, {scale = radius / 110, glow = true})
    self.effects:burst('embers', x, y - 30, 18)
    self.effects:burst('smoke', x, y - 30, 6)
    self.camera:addTrauma(0.25)
    self:flash(x, y, radius * 3)
    sound.play('explosion', x, y)
    if hostile then
        local px, py = self.player:position()
        if self.player.alive and distanceSquared(x, y, px, py) < radius * radius then
            self:hurtPlayer(damage, x, y)
        end
        return
    end
    self:strike(x, y, {reach = radius}, damage, push)
end

-- Lights the island around a burst for a moment.
function game:flash(x, y, radius)
    self.flashes[#self.flashes + 1] = {x = x, y = y, radius = radius, time = 0}
end

function game:nearFire(x, y, reach)
    return distanceSquared(x, y, self.campfire.x, self.campfire.y) < reach * reach
end

-- Picks up the wood and meat the player walks over. Wood waits on the ground while the arms are full, and meat is eaten at once.
function game:collectPickups(collector)
    local x, y = collector:position()
    for index = #self.pickups, 1, -1 do
        local piece = self.pickups[index]
        local full = piece.kind == 'wood' and collector.wood >= collector.class.capacity
        if piece.landed and not full and distanceSquared(x, y, piece.x, piece.y) < 64 * 64 then
            table.remove(self.pickups, index)
            sound.play('pickup', x, y)
            if piece.kind == 'wood' then
                collector.wood = collector.wood + 1
            else
                collector:eat()
                self.effects:number(x, y - 110, '+' .. config.food.meal, '#FFFFC46B')
            end
        end
    end
end

function game:deliverWood(carrier)
    local taken = self.campfire:feed(carrier.wood)
    if taken > 0 then
        carrier.wood = carrier.wood - taken
        sound.play('feed', self.campfire.x, self.campfire.y)
        self.effects:burst('embers', self.campfire.x, self.campfire.y - 40, 16)
        self.effects:number(self.campfire.x, self.campfire.y - 120, '+' .. taken * config.fire.woodFuel, '#FFFFD27A')
    end
end

-- Moves raiders that a growing circle swallowed back out to its edge.
function game:clearCircle()
    local fire = self.campfire
    for _, target in ipairs(self.enemies) do
        if target.alive then
            local x, y = target:position()
            if fire:contains(x, y, 30) then
                local dx, dy = x - fire.x, y - fire.y
                local length = math.max(1, math.sqrt(dx * dx + dy * dy))
                target.body:setTransform(fire.x + dx / length * (fire.radius + 40), fire.y + dy / length * (fire.radius + 40), 0)
                target:push(fire.x, fire.y, 500)
            end
        end
    end
end

-- Updates every living item of a list and drops the ones that fell, including those a blow of the player felled earlier in the frame.
local function updateList(list, dt, ...)
    for index = #list, 1, -1 do
        local item = list[index]
        if item.alive then
            item:update(dt, ...)
        end
        if not item.alive then
            table.remove(list, index)
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
    self.player:update(dt)

    local positions = {}
    for _, target in ipairs(self.enemies) do
        if target.alive then
            positions[#positions + 1] = {target:position()}
        end
    end
    -- Raiders and sheep fall to blows, fire and arrows, and their bodies are gone, so they leave their lists for the fallen before anything reads them again.
    updateList(self.enemies, dt, positions)
    updateList(self.herd, dt)
    updateList(self.projectiles, dt)
    for index = #self.fallen, 1, -1 do
        local body = self.fallen[index]
        body.unit:animate(dt)
        if body.unit.animator.finished then
            body.time = body.time + dt
            if body.time >= fadeTime then
                table.remove(self.fallen, index)
            end
        end
    end
    for index = #self.flashes, 1, -1 do
        local flash = self.flashes[index]
        flash.time = flash.time + dt
        if flash.time >= 0.35 then
            table.remove(self.flashes, index)
        end
    end
    for _, standing in ipairs(self.trees) do
        standing:update(dt)
    end
    for _, piece in ipairs(self.pickups) do
        piece:update(dt)
    end

    self.island.world:step(dt)
    self.effects:update(dt)

    local x, y = self.player:position()
    self.camera:follow(x, y, dt)
    audio.setListener(x, y)
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
    for _, piece in ipairs(self.pickups) do
        piece:draw()
    end
    for _, body in ipairs(self.fallen) do
        body.unit:draw(1 - body.time / fadeTime)
    end
    for _, animal in ipairs(self.herd) do
        animal:draw()
    end
    for _, target in ipairs(self.enemies) do
        target:draw()
    end
    self.player:draw()
    for _, flying in ipairs(self.projectiles) do
        flying:draw()
    end
    self.effects:draw()
    self.island:drawClouds(self.camera)

    self.campfire:light(self.cycle)
    local x, y = self.player:position()
    graphics2d.drawLight({x = x, y = y - 40, radius = 260, color = self.cycle:fill(lantern), intensity = 0.7})
    for _, flying in ipairs(self.projectiles) do
        flying:light(self.cycle)
    end
    for _, flash in ipairs(self.flashes) do
        graphics2d.drawLight({x = flash.x, y = flash.y, radius = flash.radius, color = self.cycle:fill(blaze), intensity = 1.4 * (1 - flash.time / 0.35)})
    end
end

function game:destroy()
    self.campfire:destroy()
end

return game
