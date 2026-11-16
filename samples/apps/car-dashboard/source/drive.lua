-- The drive that the dashboard shows: a car that follows a route through a grid of streets, speeds up on long streets and slows down before every turn, signals the turns, uses and recovers energy with the physics of a real car, warms its tyres and passes the cars of the other lanes. Everything is made from a seed, so every run drives the same way.
local m = require('haylen.math')

local Drive = {}
Drive.__index = Drive

Drive.streets = {'Harbor Avenue', 'Maple Street', 'Station Road', 'River Lane', 'Hillcrest Drive', 'Market Street', 'Elm Boulevard', 'Garden Way', 'Lakeview Road', 'Union Street', 'Orchard Lane', 'Bridge Street'}

-- The blocks of the city in meters, and the car: its mass in kilograms, the battery in kilowatt hours and the share of the braking that regeneration recovers.
Drive.block = 320
Drive.mass = 2100
Drive.capacity = 78
Drive.regeneration = 0.7

-- Speeds in meters per second: the speed a turn takes, and the braking and the acceleration a calm driver uses.
Drive.turnSpeed = 7
Drive.braking = 1.8
Drive.acceleration = 1.6

-- The trip computer starts with a trip already under way, and the clock starts at 10:24 in the morning.
Drive.startTrip = {distance = 128.4, energy = 20.3, seconds = 96 * 60}
Drive.startClock = (10 * 60 + 24) * 60

-- The four tyres in the order front left, front right, rear left and rear right, cold, in bar.
Drive.coldPressure = {2.4, 2.4, 2.3, 2.3}

-- The share of the battery that one turn of the history of consumption covers, in seconds per sample.
Drive.sampleSeconds = 0.5
Drive.historySize = 72

function Drive.new(seed)
    local self = setmetatable({}, Drive)
    self.random = m.random(seed)
    self.speed = 0
    self.acceleration = 0
    self.power = 0
    self.consumption = 0
    self.energy = Drive.capacity * 0.72
    self.trip = {distance = Drive.startTrip.distance, energy = Drive.startTrip.energy, seconds = Drive.startTrip.seconds}
    self.clock = Drive.startClock
    self.time = 0
    self.travelled = 0
    self.pressure = {}
    for index, value in ipairs(Drive.coldPressure) do
        self.pressure[index] = value
    end
    self.history = {}
    for index = 1, Drive.historySize do
        self.history[index] = 14 + 3 * math.sin(index * 0.35) + 2 * math.sin(index * 0.11)
    end
    self.sampleTime = 0
    self.sampleSum, self.sampleCount = 0, 0

    -- The route starts heading north on the first street, with the destination a few turns ahead.
    self.x, self.y = 0, 0
    self.headingX, self.headingY = 0, -1
    self.segments = {}
    self.segmentDistance = 0
    self:planDestination()

    -- The cars of the other lanes, with their distance ahead in meters and their speed against the car.
    self.traffic = {
        {lane = -1, distance = 34, offset = 0},
        {lane = 1, distance = 62, offset = 0},
    }
    return self
end

-- Plans the streets to the next destination: each segment runs a few blocks and ends with a turn to the left or the right.
function Drive:planDestination()
    local random = self.random
    for _ = 1, random:integer(4, 6) do
        self.segments[#self.segments + 1] = {
            length = Drive.block * random:integer(1, 4),
            turn = random:chance(0.5) and 'left' or 'right',
            street = Drive.streets[random:integer(1, #Drive.streets)],
            cruise = random:range(11, 22),
            destination = false,
        }
    end
    self.segments[#self.segments].destination = true
end

function Drive:segment()
    return self.segments[1]
end

-- What the driver does at the next turn, such as Turn left onto Maple Street, made once for each turn.
function Drive:instruction()
    local segment = self:segment()
    if not segment.instruction then
        local after = self.segments[2]
        segment.instruction = 'Turn ' .. segment.turn .. ' onto ' .. after.street
    end
    return segment.instruction
end

-- The meters to the next turn.
function Drive:distanceToTurn()
    return self:segment().length - self.segmentDistance
end

-- The meters to the destination.
function Drive:distanceToDestination()
    local total = self:distanceToTurn()
    if self.segments[1].destination then
        return total
    end
    for index = 2, #self.segments do
        total = total + self.segments[index].length
        if self.segments[index].destination then
            return total
        end
    end
    return total
end

-- The minutes to the destination at the pace of city traffic.
function Drive:minutesToDestination()
    return math.max(1, math.ceil(self:distanceToDestination() / 9 / 60))
end

-- The speed the car aims for: the cruise of the street, waving with the traffic, and never more than lets it slow down to the speed of the turn ahead.
function Drive:targetSpeed()
    local segment = self:segment()
    local cruise = segment.cruise * (1 + 0.12 * math.sin(self.time * 0.23) + 0.06 * math.sin(self.time * 0.71))
    local braking = math.sqrt(Drive.turnSpeed * Drive.turnSpeed + 2 * Drive.braking * math.max(0, self:distanceToTurn() - 12))
    return math.min(cruise, braking)
end

-- The power the motor gives, in kilowatts, from the drag of the air, the roll of the tyres and the change of speed, with the share that regeneration recovers when it is negative and what the car itself uses.
function Drive:motorPower(speed, acceleration)
    local air = 0.5 * 1.2 * 0.24 * 2.4 * speed * speed * speed
    local rolling = 0.009 * Drive.mass * 9.81 * speed
    local inertia = Drive.mass * acceleration * speed
    local wheels = (air + rolling + inertia) / 1000
    local motor = wheels > 0 and wheels / 0.9 or wheels * Drive.regeneration
    return motor + 0.7
end

function Drive:update(dt)
    self.time = self.time + dt
    self.clock = self.clock + dt

    -- The speed follows its target as smoothly as a person drives.
    local wanted = (self:targetSpeed() - self.speed) * 0.8
    local acceleration = m.clamp(wanted, -Drive.braking * 1.4, Drive.acceleration)
    self.acceleration = self.acceleration + (acceleration - self.acceleration) * math.min(1, dt * 3)
    self.speed = math.max(0, self.speed + self.acceleration * dt)

    local step = self.speed * dt
    self.travelled = self.travelled + step
    self.segmentDistance = self.segmentDistance + step
    self.x = self.x + self.headingX * step
    self.y = self.y + self.headingY * step
    if self.segmentDistance >= self:segment().length then
        self:turn()
    end

    self:useEnergy(dt, step)
    self:moveTraffic(dt)
    self:warmTyres(dt)
end

-- Takes the turn at the end of the street and starts the next one, and plans a new destination once the car arrives.
function Drive:turn()
    local segment = table.remove(self.segments, 1)
    self.segmentDistance = self.segmentDistance - segment.length
    local x, y = self.headingX, self.headingY
    if segment.turn == 'right' then
        self.headingX, self.headingY = -y, x
    else
        self.headingX, self.headingY = y, -x
    end
    if segment.destination or #self.segments < 3 then
        self:planDestination()
    end
end

function Drive:useEnergy(dt, step)
    self.power = self:motorPower(self.speed, self.acceleration)
    local used = self.power * dt / 3600
    self.energy = m.clamp(self.energy - used, 0, Drive.capacity)
    self.trip.energy = self.trip.energy + used
    self.trip.distance = self.trip.distance + step / 1000
    self.trip.seconds = self.trip.seconds + dt

    -- The consumption of the moment, in kilowatt hours per 100 km, joins the history once per sample.
    if self.speed > 1 then
        local instant = m.clamp(self.power / (self.speed * 3.6) * 100, -25, 45)
        self.consumption = self.consumption + (instant - self.consumption) * math.min(1, dt * 2)
        self.sampleSum = self.sampleSum + self.consumption
        self.sampleCount = self.sampleCount + 1
    end
    self.sampleTime = self.sampleTime + dt
    if self.sampleTime >= Drive.sampleSeconds and self.sampleCount > 0 then
        local history = self.history
        for index = 1, #history - 1 do
            history[index] = history[index + 1]
        end
        history[#history] = m.clamp(self.sampleSum / self.sampleCount, -20, 40)
        self.sampleTime, self.sampleSum, self.sampleCount = 0, 0, 0
    end
end

-- The share of the battery, from 0 to 1.
function Drive:charge()
    return self.energy / Drive.capacity
end

-- The kilowatt hours per 100 km of the whole trip.
function Drive:averageConsumption()
    return self.trip.energy / self.trip.distance * 100
end

-- The kilometers the battery still reaches at the average of the trip.
function Drive:range()
    return self.energy / self:averageConsumption() * 100
end

-- The cars of the other lanes drift ahead and behind, and one that leaves the view comes back far ahead in its lane.
function Drive:moveTraffic(dt)
    for index, car in ipairs(self.traffic) do
        local relative = 2.5 * math.sin(self.time * 0.17 + index * 1.9) - 0.4
        car.distance = car.distance + relative * dt
        if car.distance < 6 then
            car.distance = 95
        elseif car.distance > 110 then
            car.distance = 10
        end
    end
end

-- The tyres warm up with the speed and cool down slowly, which raises their pressure by up to a tenth of a bar.
function Drive:warmTyres(dt)
    for index, cold in ipairs(Drive.coldPressure) do
        local warm = cold + 0.1 * m.clamp(self.speed / 25, 0, 1)
        self.pressure[index] = self.pressure[index] + (warm - self.pressure[index]) * dt * 0.02
    end
end

-- Whether the turn signal is lit this moment, and the side of the turn it shows: it blinks over the last 160 meters before a turn.
function Drive:signal()
    if self:distanceToTurn() > 160 then
        return false, nil
    end
    return self.time % 0.8 < 0.45, self:segment().turn
end

return Drive
