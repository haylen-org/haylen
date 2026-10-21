-- A walker the camera tests follow: the adventurer of the walker sheet, who idles and runs, turns to face where it goes and walks around the water and the props of the island. It moves with a vector action, or toward the point the mouse button or a finger holds inside the view of its camera, and until a player steers it, it can walk a route of its own, so a test shows what it does from the start.
local animation2d = require('haylen.animation2d')
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local input = require('haylen.input')
local m = require('haylen.math')
local ui = require('haylen.ui')

local World = require('categories.camera.world')

local Walker = haylen.class('Walker')

Walker.speed = 520
Walker.scale = 0.52
-- The feet of every frame of the sheet stand on this fraction of the height of its cells of 256 pixels.
Walker.feet = 248 / 256

-- Takes the start position and the optional table `options`: the vector `action` that steers it, which a walker moved by code leaves out, the `route` it walks in a loop until a player steers it, as points with an optional pause in seconds as their third value, and the `color` of the ring it stands on.
function Walker:init(x, y, options)
    options = options or {}
    self.x, self.y = x, y
    self.vx, self.vy = 0, 0
    self.action = options.action
    self.route = options.route
    self.waypoint, self.pause = 1, 0
    self.color = options.color
    local sheet = World.texture('sprites/sheets/walker.png')
    self.sprite = graphics2d.newSprite(sheet, {scaleX = Walker.scale, scaleY = Walker.scale, layer = 1, visibility = World.details})
    self.animator = animation2d.newAnimator()
    self.animator:add('idle', animation2d.fromGrid(sheet, {frameWidth = 256, frameHeight = 256, cells = {1, 2, 3, 4}, framesPerSecond = 6}))
    self.animator:add('run', animation2d.fromGrid(sheet, {frameWidth = 256, frameHeight = 256, cells = {9, 10, 11, 12, 13, 14, 15, 16}, framesPerSecond = 14}))
    self.animator.pivotY = Walker.feet
    self.animator:play('idle')
end

-- Returns the screen point held with the left mouse button or a finger outside the interface, or nothing.
function Walker.heldPoint()
    local touch = input.touches()[1]
    if touch and touch.phase ~= 'ended' and touch.phase ~= 'cancelled' and not ui.usingPointer() then
        return touch.x, touch.y
    end
    if input.down('point') then
        return input.mousePosition()
    end
end

-- Returns the direction the player steers in: the action, or toward the point held inside the view of the camera.
function Walker:steering(camera)
    local dx, dy = 0, 0
    if self.action then
        dx, dy = input.vector(self.action)
    end
    local screenX, screenY = Walker.heldPoint()
    local inside = camera and screenX and camera.viewport and camera.viewport:contains({screenX, screenY})
    if inside and dx == 0 and dy == 0 then
        local x, y = camera:screenToWorld(screenX, screenY)
        local direction = m.vec2(x - self.x, y - self.y)
        if direction:length() > 20 then
            dx, dy = direction:normalized():unpack()
        end
    end
    return dx, dy
end

-- Returns the direction toward the next point of the route, after the pause at the last one.
function Walker:alongRoute(dt)
    if self.pause > 0 then
        self.pause = self.pause - dt
        return 0, 0
    end
    local point = self.route[self.waypoint]
    local dx, dy = point[1] - self.x, point[2] - self.y
    local distance = math.sqrt(dx * dx + dy * dy)
    if distance < Walker.speed * dt then
        self.waypoint = self.waypoint % #self.route + 1
        self.pause = point[3] or 0
        return 0, 0
    end
    return dx / distance, dy / distance
end

-- Moves for a frame within the world. A player who steers takes the walker off its route for good.
function Walker:update(dt, world, camera)
    local dx, dy = self:steering(camera)
    if dx ~= 0 or dy ~= 0 then
        self.route = nil
    elseif self.route then
        dx, dy = self:alongRoute(dt)
    end

    local stepX, stepY = dx * Walker.speed * dt, dy * Walker.speed * dt
    local startX, startY = self.x, self.y
    if stepX ~= 0 and world:walkable(self.x + stepX, self.y) then
        self.x = self.x + stepX
    end
    if stepY ~= 0 and world:walkable(self.x, self.y + stepY) then
        self.y = self.y + stepY
    end
    -- A prop in the way of the route sends the walker on to the next point of it.
    if self.route and (dx ~= 0 or dy ~= 0) and self.x == startX and self.y == startY then
        self.waypoint = self.waypoint % #self.route + 1
    end
    self:moved(startX, startY, dx, dt)
end

-- Places a walker that code moves, such as one that wanders by itself.
function Walker:moveTo(x, y, dt)
    local startX, startY = self.x, self.y
    self.x, self.y = x, y
    self:moved(startX, startY, x - startX, dt)
end

-- Keeps the velocity, the clip and the facing in step with the last move.
function Walker:moved(startX, startY, facing, dt)
    if dt > 0 then
        self.vx, self.vy = (self.x - startX) / dt, (self.y - startY) / dt
    end
    local clip = math.abs(self.vx) + math.abs(self.vy) > 20 and 'run' or 'idle'
    if self.animator.current ~= clip then
        self.animator:play(clip)
    end
    if math.abs(facing) > 0.05 then
        self.sprite.flipHorizontal = facing < 0
    end
    self.animator:update(dt)
end

-- Draws the walker on its feet, with its shadow and its ring on the ground below the props.
function Walker:draw()
    local x, y = self.x, self.y
    graphics2d.drawShape({x - 38, y - 12, 76, 24}, {kind = 'ellipse', color = '#48102010', softness = 8, layer = -1, visibility = World.details})
    if self.color then
        local border = math.max(6, 3 * graphics2d.canvasUnitSize())
        graphics2d.drawShape({x - 46, y - 17, 92, 34}, {kind = 'ellipse', color = '#00000000', borderWidth = border, borderColor = self.color, layer = -1, visibility = World.details})
    end
    self.sprite.x, self.sprite.y = x, y
    self.animator:apply(self.sprite)
    self.sprite:draw()
end

return Walker
