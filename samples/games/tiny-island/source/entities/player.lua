-- The survivor the player controls. Attacking next to a tree chops it, and anywhere else it fights with the move of the chosen class: the slash of the warrior, the arrows of the archer, the thrust of the lancer or the fireball of the mage.
local input = require('haylen.input')

local config = require('config')
local sound = require('systems.sound')
local unit = require('entities.unit')

local player = setmetatable({}, {__index = unit})
player.__index = player

local chopReach = 120
-- A raider this close is fought first, even with a tree in reach.
local threatReach = 170

function player.new(game, class, x, y)
    local self = setmetatable({game = game, class = class, timers = {}}, player)
    local category = config.category
    unit.setup(self, game.island.world, class.id, x, y, 22, category.player, category.world | category.enemy | category.tree | category.sheep)
    self.health = class.health
    self.maxHealth = class.health
    self.food = config.food.start
    self.wood = 0
    self.cooldown = 0
    self.specialCooldown = 0
    self.chargeTime = 0
    self.stepTime = 0
    self.aim = {x = 1, y = 0}
    return self
end

-- Runs `action` after `delay` seconds of the run, so a blow lands on the frame of the animation that shows it.
function player:after(delay, action)
    self.timers[#self.timers + 1] = {time = delay, action = action}
end

function player:busy()
    return unit.busy(self, 'attack') or unit.busy(self, 'cast')
end

-- Faces the closest raider within reach, or else the closest sheep, so attacks go where the danger or the dinner is.
function player:aimAt(reach)
    local x, y = self:position()
    local target = self.game:closestEnemy(x, y, reach) or self.game:closestSheep(x, y, reach)
    if target then
        local tx, ty = target:position()
        local dx, dy = tx - x, ty - y
        local length = math.max(1, math.sqrt(dx * dx + dy * dy))
        self.aim = {x = dx / length, y = dy / length}
        if math.abs(dx) > 1 then
            self.facing = dx > 0 and 1 or -1
        end
    end
    return target
end

local attacks = {}

-- A wide slash in front that knocks raiders back.
function attacks.slash(self)
    self:after(0.12, function()
        local x, y = self:position()
        local aim = self.aim
        self.game.effects:animate('slash', x + aim.x * 64, y - 48 + aim.y * 36, {rotation = math.atan(aim.y, aim.x), scale = 0.95, glow = true})
        self.game:strike(x, y, {reach = self.class.range, direction = aim}, self.class.damage, 620)
    end)
end

function attacks.arrow(self)
    self:after(0.2, function()
        local x, y = self:position()
        self.game:shoot('arrow', self, x + self.aim.x * 30, y - 58, self.aim.x, self.aim.y, self.class.damage, {pierce = 2})
    end)
end

-- A long straight thrust that reaches past the tip of the spear.
function attacks.thrust(self)
    self:after(0.16, function()
        local x, y = self:position()
        local aim = self.aim
        self.game.effects:animate('thrust', x + aim.x * 110, y - 50 + aim.y * 60, {rotation = math.atan(aim.y, aim.x), scale = 0.9, glow = true})
        self.game:strike(x, y, {reach = self.class.range, direction = aim, width = 52}, self.class.damage, 420)
    end)
end

-- A fireball that bursts on the first raider it meets and burns everything around it.
function attacks.fireball(self)
    sound.play('heal', self:position())
    self:after(0.22, function()
        local x, y = self:position()
        self.game:shoot('fireball', self, x + self.aim.x * 50, y - 62, self.aim.x, self.aim.y, self.class.damage)
    end)
end

function player:attack()
    local x, y = self:position()
    self.cooldown = self.class.cooldown
    local threatened = self.game:closestEnemy(x, y, threatReach) or self.game:closestSheep(x, y, threatReach * 0.7)
    local tree = not threatened and self.game:treeInReach(x, y, chopReach, self.facing)
    self:play('attack', true)
    sound.play('swing', x, y)

    if tree then
        self:after(0.14, function()
            self.game:chop(tree, self.class.chop)
        end)
        return
    end
    self:aimAt(self.class.range + 220)
    attacks[self.class.attack](self)
end

local specials = {}

function specials.volley(self)
    self:aimAt(self.class.range)
    self:after(0.3, function()
        local x, y = self:position()
        local angle = math.atan(self.aim.y, self.aim.x)
        for offset = -1, 1 do
            self.game:shoot('arrow', self, x, y - 58, math.cos(angle + offset * 0.2), math.sin(angle + offset * 0.2), self.class.damage, {pierce = 2})
        end
    end)
end

-- A dash that hurts everything on the way, once per charge.
function specials.charge(self)
    self:aimAt(self.class.range + 300)
    self.chargeTime = 0.3
    self.chargeHits = {}
    self.knockback = {x = self.aim.x * 1150, y = self.aim.y * 1150}
    sound.play('swing', self:position())
end

-- A ring of fire that bursts around the mage and throws every raider back.
function specials.nova(self)
    sound.play('heal', self:position())
    self:after(0.3, function()
        local x, y = self:position()
        self.game.effects:burst('flames', x, y - 30, 70)
        self.game.effects:burst('embers', x, y - 30, 30)
        self.game:strike(x, y, {reach = 200}, self.class.damage, 700)
        self.game:flash(x, y, 420)
        self.game.camera:addTrauma(0.3)
        sound.play('explosion', x, y)
    end)
end

function player:special()
    self.specialCooldown = self.class.specialCooldown
    self:play('cast', true)
    specials[self.class.special](self)
end

-- Counts down the delayed actions of the moves. An action may start another, which waits for the next frame.
function player:runTimers(dt)
    local due = {}
    for index = #self.timers, 1, -1 do
        local timer = self.timers[index]
        timer.time = timer.time - dt
        if timer.time <= 0 then
            table.remove(self.timers, index)
            due[#due + 1] = timer.action
        end
    end
    for index = #due, 1, -1 do
        due[index]()
    end
end

function player:update(dt)
    if not self.alive then
        self:animate(dt)
        return
    end
    local x, y = self:position()
    self.cooldown = math.max(0, self.cooldown - dt)
    self.specialCooldown = math.max(0, self.specialCooldown - dt)
    self:runTimers(dt)
    self:digest(dt)
    if not self.alive then
        return
    end

    local guardHeld = self.class.special == 'guard' and input.down('special')
    if guardHeld and not self.guarding then
        self:play('cast', true)
    end
    self.guarding = guardHeld

    local mx, my = input.vector('move')
    local length = math.sqrt(mx * mx + my * my)
    if length > 0.2 then
        self.aim = {x = mx / length, y = my / length}
    end
    local speed = self.class.speed * (self.guarding and 0.35 or 1) * (self:busy() and 0.55 or 1)
    unit.move(self, mx * speed, my * speed, dt)

    if self.chargeTime > 0 then
        self.chargeTime = self.chargeTime - dt
        self.game.effects:burst('dust', x, y, 1)
        self.game:strike(x, y, {reach = 95}, self.class.damage * 0.6, 680, self.chargeHits)
    end

    if input.pressed('attack') and self.cooldown <= 0 and not self.guarding then
        self:attack()
    elseif self.class.special ~= 'guard' and input.pressed('special') and self.specialCooldown <= 0 then
        self:special()
    end

    -- Walking up to the fire feeds it, and the interact action reaches it from further away.
    local reach = input.down('interact') and config.fire.feedDistance * 2 or config.fire.feedDistance
    if self.wood > 0 and self.game:nearFire(x, y, reach) then
        self.game:deliverWood(self)
    end
    self.game:collectPickups(self)

    local moving = length > 0.2
    if not self:busy() and not self.guarding and not unit.busy(self, 'hurt') then
        self:play(moving and 'run' or 'idle')
    end
    if moving then
        self.stepTime = self.stepTime - dt
        if self.stepTime <= 0 then
            self.stepTime = 0.32
            sound.play('footstep', x, y)
            self.game.effects:burst('dust', x, y, 1)
        end
    end
    self:animate(dt)
end

-- Food runs down all the time, and an empty belly costs health until the survivor eats.
function player:digest(dt)
    local food = config.food
    self.food = math.max(0, self.food - food.hunger * dt)
    if self.food <= 0 then
        if not self.starving then
            self.starving = true
            self.game:notify('hud.starving')
        end
        if self:hurt(food.starving * dt) then
            self.game:playerFell()
        end
    else
        self.starving = false
    end
end

function player:eat()
    local food = config.food
    self.food = math.min(food.max, self.food + food.meal)
    self.health = math.min(self.maxHealth, self.health + food.mealHealth)
end

-- Takes a hit, which a raised guard mostly blocks, and returns `true` when the player falls.
function player:damage(amount, fromX, fromY)
    local x, y = self:position()
    if self.guarding then
        amount = amount * 0.3
        sound.play('guard', x, y)
        self.game.effects:burst('sparks', x + self.facing * 30, y - 60, 10)
    else
        sound.play('playerHurt', x, y)
        self.game.effects:animate('hit', x, y - 50, {scale = 0.6, glow = true, color = '#FFFFB0A0'})
        if not self:busy() then
            self:play('hurt', true)
        end
    end
    self:push(fromX, fromY, 380)
    self.game.effects:number(x, y - 90, tostring(math.floor(amount + 0.5)), '#FFFF7A6B')
    self.game.camera:addTrauma(0.35)
    return self:hurt(amount)
end

-- Falls to the ground with the last animation of the class, which the run waits for before it ends.
function player:fall()
    self.timers = {}
    self:play('death', true)
    unit.destroy(self)
end

return player
