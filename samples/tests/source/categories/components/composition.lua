-- Composition: an entity made of small behaviors, each of which does one thing, so the same behavior serves entities of every kind: a bunny that wanders and bobs, a knight that follows the cursor, a rock that shoots and a gem that circles the knight.
-- The module needs only the engine and `health-bar.lua`, so it copies with that file into another project, with the path of its `require` changed to where that file lands. Make an entity with `Composition.entity(name, picture, x, y)`, give it behaviors with `entity:add(Composition.wander({speed = 80}))`, and call `entity:update(dt)` and `entity:draw()` every frame. A behavior is a table with any of `attach(entity)`, `update(entity, dt)`, `draw(entity)` and `hit(entity, amount)`, so a game adds its own the same way.
local graphics2d = require('haylen.graphics2d')

local HealthBar = require('categories.components.health-bar')

local Composition = {}

local Entity = {}
Entity.__index = Entity

-- Makes an entity at `x`, `y` that draws `picture`, a texture, or the region `source` of it, such as a frame of an atlas, standing on that point at `scale`.
function Composition.entity(name, picture, x, y, scale, source)
    scale = scale or 1
    local width, height = picture.width, picture.height
    if source then
        width, height = source.width, source.height
    end
    return setmetatable({name = name, picture = picture, x = x, y = y, scale = scale, lift = 0, size = {width * scale, height * scale}, behaviors = {}, names = {}, order = {source = source, pivotY = 1, scaleX = scale, scaleY = scale, layer = 1}}, Entity)
end

function Entity:add(behavior)
    self.behaviors[#self.behaviors + 1] = behavior
    self.names[#self.names + 1] = behavior.name
    if behavior.attach then
        behavior.attach(self)
    end
    return self
end

-- Removes the first behavior with a name, such as when a power-up ends.
function Entity:remove(name)
    for index, behavior in ipairs(self.behaviors) do
        if behavior.name == name then
            table.remove(self.behaviors, index)
            table.remove(self.names, index)
            return true
        end
    end
    return false
end

function Entity:has(name)
    for _, behavior in ipairs(self.behaviors) do
        if behavior.name == name then
            return true
        end
    end
    return false
end

function Entity:update(dt)
    for _, behavior in ipairs(self.behaviors) do
        if behavior.update then
            behavior.update(self, dt)
        end
    end
end

-- Hands a hit to every behavior that answers hits, such as health, and answers whether one did.
function Entity:hit(amount)
    local answered = false
    for _, behavior in ipairs(self.behaviors) do
        if behavior.hit then
            behavior.hit(self, amount)
            answered = true
        end
    end
    return answered
end

function Entity:draw()
    graphics2d.draw(self.picture, self.x, self.y - self.lift, self.order)
    for _, behavior in ipairs(self.behaviors) do
        if behavior.draw then
            behavior.draw(self)
        end
    end
end

function Entity:width()
    return self.size[1]
end

function Entity:height()
    return self.size[2]
end

-- Walks to random points inside `area`, `{left, top, width, height}`, at `speed` units per second.
function Composition.wander(options)
    local behavior = {name = 'wander', speed = options.speed or 80, area = options.area, goalX = nil, goalY = nil, seed = options.seed or 7}
    local function roll()
        behavior.seed = (behavior.seed * 1103515245 + 12345) % 2147483648
        return behavior.seed / 2147483648
    end
    function behavior.update(entity, dt)
        local area = behavior.area
        if not behavior.goalX or math.abs(entity.x - behavior.goalX) + math.abs(entity.y - behavior.goalY) < 6 then
            behavior.goalX, behavior.goalY = area[1] + roll() * area[3], area[2] + roll() * area[4]
        end
        local dx, dy = behavior.goalX - entity.x, behavior.goalY - entity.y
        local distance = math.sqrt(dx * dx + dy * dy)
        if distance > 0 then
            local step = math.min(distance, behavior.speed * dt)
            entity.x, entity.y = entity.x + dx / distance * step, entity.y + dy / distance * step
        end
    end
    return behavior
end

-- Follows `target`, any table with `x` and `y`, and stops `distance` units from it.
function Composition.follow(options)
    local behavior = {name = 'follow', target = options.target, speed = options.speed or 260, distance = options.distance or 60}
    function behavior.update(entity, dt)
        local dx, dy = behavior.target.x - entity.x, behavior.target.y - entity.y
        local distance = math.sqrt(dx * dx + dy * dy)
        if distance > behavior.distance then
            local step = math.min(distance - behavior.distance, behavior.speed * dt)
            entity.x, entity.y = entity.x + dx / distance * step, entity.y + dy / distance * step
        end
    end
    return behavior
end

-- Circles `center`, an entity or any table with `x` and `y`, at `radius` units, `speed` turns per second.
function Composition.orbit(options)
    local behavior = {name = 'orbit', center = options.center, radius = options.radius or 120, speed = options.speed or 0.4, angle = 0}
    function behavior.update(entity, dt)
        behavior.angle = behavior.angle + behavior.speed * math.pi * 2 * dt
        entity.x = behavior.center.x + math.cos(behavior.angle) * behavior.radius
        entity.y = behavior.center.y - 60 + math.sin(behavior.angle) * behavior.radius * 0.45
    end
    return behavior
end

-- Bobs the drawing up and down by `height` units, `speed` times per second.
function Composition.bob(options)
    local behavior = {name = 'bob', height = options.height or 12, speed = options.speed or 1.2, time = options.phase or 0}
    function behavior.update(entity, dt)
        behavior.time = behavior.time + dt
        entity.lift = (math.sin(behavior.time * behavior.speed * math.pi * 2) * 0.5 + 0.5) * behavior.height
    end
    return behavior
end

-- Fires a shot at `target` every `interval` seconds, and moves and draws its own shots.
function Composition.shooter(options)
    local behavior = {name = 'shooter', target = options.target, interval = options.interval or 1.2, speed = options.speed or 520, wait = options.interval or 1.2, shots = {}, color = options.color or '#FFF2B23A'}
    function behavior.update(entity, dt)
        behavior.wait = behavior.wait - dt
        if behavior.wait <= 0 then
            behavior.wait = behavior.interval
            local fromY = entity.y - entity:height() * 0.6
            local dx, dy = behavior.target.x - entity.x, behavior.target.y - 80 - fromY
            local distance = math.sqrt(dx * dx + dy * dy) + 1
            behavior.shots[#behavior.shots + 1] = {x = entity.x, y = fromY, vx = dx / distance * behavior.speed, vy = dy / distance * behavior.speed, life = 2}
        end
        for index = #behavior.shots, 1, -1 do
            local shot = behavior.shots[index]
            shot.x, shot.y, shot.life = shot.x + shot.vx * dt, shot.y + shot.vy * dt, shot.life - dt
            if shot.life <= 0 then
                table.remove(behavior.shots, index)
            end
        end
    end
    function behavior.draw()
        for _, shot in ipairs(behavior.shots) do
            graphics2d.drawCircle(shot.x, shot.y, 10, behavior.color, {layer = 3})
        end
    end
    return behavior
end

-- Gives the entity health with a bar over its head, and hands every hit to `onHit(entity, taken)`, such as to show a label.
function Composition.health(options)
    local behavior = {name = 'health', bar = HealthBar.new(options.maximum or 60, {width = options.width or 120, height = 14, tick = options.tick or 0}), onHit = options.onHit}
    function behavior.update(_, dt)
        behavior.bar:update(dt)
    end
    function behavior.hit(entity, amount)
        local taken = behavior.bar:damage(amount)
        if behavior.onHit then
            behavior.onHit(entity, taken)
        end
    end
    function behavior.draw(entity)
        behavior.bar:draw(entity.x, entity.y - entity.lift - entity:height() - 22)
    end
    return behavior
end

return Composition
