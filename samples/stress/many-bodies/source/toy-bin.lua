-- The toy bin: a wooden box that three chutes pour toy blocks, balls, gems and pills into, every toy a Box2D body. The bodies reach the screen through one sprite batch: one "readTransforms" call copies every position into a float buffer and one "writeFields" call moves the sprites, so no Lua runs per body while the bin draws.
local collections = require('haylen.collections')
local graphics2d = require('haylen.graphics2d')
local physics2d = require('haylen.physics2d')

local ToyBin = {}
ToyBin.__index = ToyBin

-- The inside of the bin, with its floor at y 0 and its walls at the sides, and the thickness of its planks.
ToyBin.width = 5600
ToyBin.height = 3800
ToyBin.plank = 90
ToyBin.chutes = {-1900, 0, 1900}
ToyBin.mouth = 340
ToyBin.rowSize = 10
ToyBin.dropSpeed = 1200
ToyBin.fields = {'x', 'y', 'rotation'}
ToyBin.material = {friction = 0.5, restitution = 0.15, contactEvents = false, hitEvents = false, sensorEvents = false}
ToyBin.ballMaterial = {friction = 0.4, restitution = 0.45, rollingResistance = 0.04, contactEvents = false, hitEvents = false, sensorEvents = false}
ToyBin.blast = {radius = 560, impulse = 360, falloff = 'linear'}
ToyBin.shakeDuration = 2.2
ToyBin.shakeAmplitude = 26
ToyBin.shakeFrequency = 6
ToyBin.order = {layer = 2}
-- A little spin damping and a sleep speed of a tenth of a meter per second let a pile of rounded toys come to rest and fall asleep.
ToyBin.angularDamping = 0.8
ToyBin.sleepThreshold = 6.4

-- The toys, with their weight in the mix, their size in world units and the row of the atlas, whose cells of 64 units sit 72 units apart and rasterize at twice their size.
ToyBin.kinds = {
    {name = 'block', weight = 3, width = 32, height = 32, row = 0},
    {name = 'ball', weight = 3, width = 32, height = 32, row = 1},
    {name = 'gem', weight = 2, width = 34, height = 34, row = 2},
    {name = 'pill', weight = 2, width = 44, height = 22, row = 3},
}
ToyBin.cell = 144
ToyBin.gutter = 8

function ToyBin.new(texture, random)
    local self = setmetatable({}, ToyBin)
    self.random = random
    self.world = physics2d.newWorld({interpolate = true})
    self.bodies = {}
    self.pending = 0
    self.shake = 0
    self.row = 0
    self.weights = {}
    for index, kind in ipairs(ToyBin.kinds) do
        self.weights[index] = kind.weight
    end
    self.batch = graphics2d.newSpriteBatch(texture)
    self.transforms = collections.newFloatBuffer(0)
    self:buildBin()
    return self
end

-- The bin is one kinematic body of planks, so the shake moves it as a whole and it carries every toy it holds. The lid leaves a gap under every chute.
function ToyBin:buildBin()
    local width, height, plank = ToyBin.width, ToyBin.height, ToyBin.plank
    local bin = self.world:createBody({type = 'kinematic'})
    self.planks = {
        {x = -width / 2 - plank, y = 0, width = width + plank * 2, height = plank},
        {x = -width / 2 - plank, y = -height - plank, width = plank, height = height + plank},
        {x = width / 2, y = -height - plank, width = plank, height = height + plank},
    }
    local edges = {-width / 2}
    for _, chute in ipairs(ToyBin.chutes) do
        edges[#edges + 1] = chute - ToyBin.mouth / 2
        edges[#edges + 1] = chute + ToyBin.mouth / 2
    end
    edges[#edges + 1] = width / 2
    for index = 1, #edges, 2 do
        self.planks[#self.planks + 1] = {x = edges[index], y = -height - plank, width = edges[index + 1] - edges[index], height = plank}
    end
    for _, rect in ipairs(self.planks) do
        bin:addBox(rect.width, rect.height, {offsetX = rect.x + rect.width / 2, offsetY = rect.y + rect.height / 2, friction = 0.6, contactEvents = false, hitEvents = false, sensorEvents = false})
    end
    self.bin = bin
end

function ToyBin:count()
    return #self.bodies
end

-- Queues toys that the chutes pour in rows over the next frames.
function ToyBin:pour(amount)
    self.pending = self.pending + amount
end

function ToyBin:sprite(kind, color)
    local size = ToyBin.cell / 2 - ToyBin.gutter
    local rowHeight = kind.row == 3 and size / 2 or size
    return {
        width = kind.width,
        height = kind.height,
        source = {ToyBin.gutter + color * ToyBin.cell, ToyBin.gutter + kind.row * ToyBin.cell, size * 2, rowHeight * 2},
    }
end

function ToyBin:spawn(kindIndex, x, y)
    local random = self.random
    local kind = ToyBin.kinds[kindIndex]
    local body = self.world:createBody({x = x, y = y, rotation = random:range(0, math.pi * 2), vx = random:range(-60, 60), vy = ToyBin.dropSpeed + random:range(-80, 80), angularVelocity = random:range(-4, 4), angularDamping = ToyBin.angularDamping, sleepThreshold = ToyBin.sleepThreshold})
    if kind.name == 'block' then
        body:addBox(kind.width, kind.height, ToyBin.material)
    elseif kind.name == 'ball' then
        body:addCircle(kind.width / 2, ToyBin.ballMaterial)
    elseif kind.name == 'gem' then
        -- The corners of the gem art touch a circle of 31 units in its cell of 64.
        local radius = kind.width * 31 / 64
        local points = {}
        for corner = 0, 5 do
            local angle = corner * math.pi / 3
            points[#points + 1] = {math.cos(angle) * radius, math.sin(angle) * radius}
        end
        body:addPolygon(points, ToyBin.material)
    else
        local reach = (kind.width - kind.height) / 2
        body:addCapsule(-reach, 0, reach, 0, kind.height / 2, ToyBin.material)
    end
    self.bodies[#self.bodies + 1] = body
    self.batch:add(self:sprite(kind, random:integer(0, 3)))
end

-- Every chute drops a row of toys across its mouth every second frame, so a row clears the next one before it arrives.
function ToyBin:spawnRows()
    self.row = self.row + 1
    if self.pending <= 0 or self.row % 2 == 1 then
        return
    end
    local random = self.random
    local spacing = ToyBin.mouth / ToyBin.rowSize
    for _, chute in ipairs(ToyBin.chutes) do
        for slot = 1, ToyBin.rowSize do
            if self.pending <= 0 then
                break
            end
            local x = chute - ToyBin.mouth / 2 + (slot - 0.5) * spacing + random:range(-4, 4)
            self:spawn(random:weightedIndex(self.weights), x, -ToyBin.height + 30 + random:range(-6, 6))
            self.pending = self.pending - 1
        end
    end
    self:fitTransforms()
end

-- Removes the newest toys first, and the toys still waiting in the chutes before them.
function ToyBin:remove(amount)
    local waiting = math.min(self.pending, amount)
    self.pending = self.pending - waiting
    local count = #self.bodies
    local keep = math.max(0, count - (amount - waiting))
    for index = count, keep + 1, -1 do
        self.bodies[index]:destroy()
        self.bodies[index] = nil
    end
    self.batch:resize(keep)
    self:fitTransforms()
end

-- Removes the toy at `index`, which the last toy takes the place of, such as a toy a blast threw out of the bin.
function ToyBin:removeAt(index)
    local last = #self.bodies
    self.bodies[index]:destroy()
    if index ~= last then
        self.bodies[index] = self.bodies[last]
        local sprite = self.batch:get(last)
        self.batch:set(index, {width = sprite.width, height = sprite.height, source = sprite.source})
    end
    self.bodies[last] = nil
    self.batch:remove(last)
end

-- The float buffer grows in steps, since the bodies copy only as many transforms as they are.
function ToyBin:fitTransforms()
    local needed = #self.bodies * 3
    if #self.transforms < needed then
        self.transforms = collections.newFloatBuffer(math.max(needed, #self.transforms * 2))
    end
end

-- Pushes every toy within the blast radius of `x`, `y` away from it, and returns how many it reached.
function ToyBin:explode(x, y)
    local blast = ToyBin.blast
    return #physics2d.explode(self.world, {x = x, y = y, radius = blast.radius, impulse = blast.impulse, falloff = blast.falloff})
end

function ToyBin:startShake()
    self.shake = ToyBin.shakeDuration
end

-- Steps the world, moving the bin through its shake first. The shake fades out, and the last step brings the bin back to rest where it started.
function ToyBin:step(step)
    if self.shake > 0 then
        self.shake = math.max(0, self.shake - step)
        local fade = self.shake / ToyBin.shakeDuration
        local offset = math.sin((ToyBin.shakeDuration - self.shake) * ToyBin.shakeFrequency * math.pi * 2) * ToyBin.shakeAmplitude * fade
        self.bin:moveTo(offset, math.abs(offset) * 0.3, 0)
    elseif self.bin.x ~= 0 or self.bin.y ~= 0 then
        self.bin:moveTo(0, 0, 0)
    end
    self.world:step(step)
end

-- Copies the transforms of every toy into the sprite batch.
function ToyBin:sync()
    if #self.bodies == 0 then
        return
    end
    self.world:readTransforms(self.bodies, self.transforms, 1, true)
    self.batch:writeFields(self.transforms, ToyBin.fields)
end

-- Removes the toys that a blast threw out through a gap of the lid, read from the transforms of the last sync.
function ToyBin:removeEscaped()
    local transforms = self.transforms
    local limitX, top = ToyBin.width / 2 + ToyBin.plank * 3, -ToyBin.height - ToyBin.plank * 6
    for index = #self.bodies, 1, -1 do
        local x, y = transforms[index * 3 - 2], transforms[index * 3 - 1]
        if y < top or y > ToyBin.plank * 3 or x < -limitX or x > limitX then
            self:removeAt(index)
        end
    end
end

function ToyBin:draw(planks)
    local x, y = self.bin:renderTransform()
    for _, rect in ipairs(self.planks) do
        graphics2d.drawNineSlice(planks, {rect.x + x, rect.y + y, rect.width, rect.height}, nil, {layer = 3}, 0.6)
    end
    if #self.bodies > 0 then
        self.batch:draw(ToyBin.order)
    end
end

return ToyBin
