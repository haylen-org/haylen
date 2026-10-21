-- A point that walks over the surface of a cave at a fixed height, around rocks, up walls and across ceilings. Each step moves it along the surface and puts it back at its height, so it rounds outer corners and inner corners alike. Given a target, it traces the outline of the surface it stands on once and walks the shorter way around to the point of that outline nearest the target, so no bump of the rock ever holds it back, and `unreachable` tells when the target lies on another surface. It speeds up and slows down smoothly, stops before it turns around, and wanders when it has no target.
local haylen = require('haylen')
local m = require('haylen.math')

local Crawler = haylen.class('Crawler')

Crawler.arrival = 6
Crawler.turnRate = 10
Crawler.acceleration = 6
-- The crawler brakes over the distance it covers in this many seconds at its speed, and never harder than this many times its speed each second.
Crawler.brakingTime = 0.35
Crawler.deceleration = 4
Crawler.traceStep = 8
Crawler.traceLimit = 1500
-- The trace points around the last known one that the crawler searches for where it is now.
Crawler.trackWindow = 6

function Crawler:init(cave, x, y, height, speed, seed)
    self.cave, self.height, self.speed = cave, height, speed
    self.random = m.random(seed)
    self.dir = 1
    self.velocity = 0
    self.wanderTime = self.random:range(3, 7)
    self.moved = 0
    self.travelled = 0
    self:land(x, y)
end

-- Returns the direction of travel along the surface.
function Crawler:tangent()
    return -self.ny * self.dir, self.nx * self.dir
end

-- Places the crawler on the surface under a point at once, such as when a leap or a turn lands, and finds its place on the route again.
function Crawler:land(x, y)
    self.x, self.y = self.cave:project(x, y, self.height)
    self.nx, self.ny = self.cave:normal(self.x, self.y)
    if self.route then
        self.route.index = self:nearestIndex(1, #self.route.points)
    end
end

-- Walks the outline of the surface under the crawler from where it stands in the direction of `dir` 1 and returns its points, `traceStep` apart, and whether the outline closed into a loop.
function Crawler:trace()
    local cave, step, height = self.cave, Crawler.traceStep, self.height
    local points = {{self.x, self.y}}
    local x, y = self.x, self.y
    local left = false
    for index = 2, Crawler.traceLimit do
        local nx, ny = cave:normal(x, y)
        x, y = cave:project(x - ny * step, y + nx * step, height)
        -- The outline closes once the trace has left the start and comes back to it.
        local back = (x - self.x) ^ 2 + (y - self.y) ^ 2
        left = left or back > (step * 4) ^ 2
        if left and back < (step * 1.5) ^ 2 then
            return points, true
        end
        points[index] = {x, y}
    end
    return points, false
end

-- Returns the index of the trace point nearest the crawler among `count` points from `first` on, wrapping around the loop.
function Crawler:nearestIndex(first, count)
    local points = self.route.points
    local best, bestDistance = first, math.huge
    for offset = 0, count - 1 do
        local index = (first + offset - 1) % #points + 1
        local distance = (points[index][1] - self.x) ^ 2 + (points[index][2] - self.y) ^ 2
        if distance < bestDistance then
            best, bestDistance = index, distance
        end
    end
    return best
end

-- Plans the way to a target, or clears it without one: the point of the outline nearest the target and the shorter way around to it.
function Crawler:setTarget(x, y)
    if not x then
        self.route = nil
        return
    end
    local points, closed = self:trace()
    local goal, goalDistance = 1, math.huge
    for index, point in ipairs(points) do
        local distance = (point[1] - x) ^ 2 + (point[2] - y) ^ 2
        if distance < goalDistance then
            goal, goalDistance = index, distance
        end
    end
    local forward = goal - 1
    local dir = (closed and #points - forward < forward) and -1 or 1
    self.route = {points = points, closed = closed, goal = goal, dir = dir, index = 1}
    -- A target whose own spot over the rock is away from the outline lies on another surface.
    local spotX, spotY = self.cave:project(x, y, self.height)
    self.unreachable = (spotX - points[goal][1]) ^ 2 + (spotY - points[goal][2]) ^ 2 > (Crawler.traceStep * 3) ^ 2
end

-- Returns how far the crawler still has to go along the route, in trace steps.
function Crawler:remainingSteps()
    local route = self.route
    local count = #route.points
    local steps = (route.goal - route.index) * route.dir
    return route.closed and steps % count or math.max(steps, 0)
end

function Crawler:update(dt)
    local cave = self.cave
    local desired, dir = self.speed, self.dir
    local route = self.route
    local braking = math.max(Crawler.arrival * 4, self.speed * Crawler.brakingTime)
    self.arrived = false
    if route then
        route.index = self:nearestIndex(route.index - Crawler.trackWindow, Crawler.trackWindow * 2 + 1)
        local goal = route.points[route.goal]
        local away = math.sqrt((goal[1] - self.x) ^ 2 + (goal[2] - self.y) ^ 2)
        local remaining = self:remainingSteps() * Crawler.traceStep
        dir = route.dir
        -- Near the goal the crawler measures what is left ahead of it by the goal itself, so it settles on it and never turns back for it.
        if remaining < braking and dir == self.dir then
            remaining = math.max(0, (-self.ny * (goal[1] - self.x) + self.nx * (goal[2] - self.y)) * self.dir)
        end
        desired = self.speed * m.clamp(remaining / braking, 0, 1)
        self.arrived = away <= Crawler.arrival or (remaining == 0 and away < braking)
        if self.arrived then
            desired = 0
        end
    else
        self.wanderTime = self.wanderTime - dt
        if self.wanderTime <= 0 then
            self.wanderTime = self.random:range(3, 7)
            if self.random:chance(0.4) then
                dir = -self.dir
            end
        end
    end

    -- A turn around waits until the crawler has stopped.
    if dir ~= self.dir then
        desired = 0
        if self.velocity < 2 then
            self.dir = dir
        end
    end
    if desired > self.velocity then
        self.velocity = self.velocity + (desired - self.velocity) * m.dampFactor(Crawler.acceleration, dt)
    else
        self.velocity = math.max(desired, self.velocity - self.speed * Crawler.deceleration * dt)
    end

    local step = self.velocity * dt
    local nx, ny = cave:normal(self.x, self.y)
    self.x, self.y = cave:project(self.x - ny * self.dir * step, self.y + nx * self.dir * step, self.height)
    self.moved = step
    self.travelled = self.travelled + step
    nx, ny = cave:normal(self.x, self.y)
    -- The normal that bodies draw with turns smoothly, so a sharp inner corner never snaps them around.
    local blend = m.dampFactor(Crawler.turnRate, dt)
    self.nx, self.ny = self.nx + (nx - self.nx) * blend, self.ny + (ny - self.ny) * blend
    local length = math.sqrt(self.nx * self.nx + self.ny * self.ny)
    self.nx, self.ny = self.nx / length, self.ny / length
end

return Crawler
