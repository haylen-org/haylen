-- The traffic of the city: cars that ride the shortest routes between random corners, and pick another route when they arrive, so the map lives behind every screen.
local haylen = require('haylen')

local City = require('city')
local Random = require('random')

local Traffic = haylen.class('Traffic')

Traffic.speed = 90
-- The colors of the cars, soft enough that the car of the driver stands out in the yellow of the app.
Traffic.colors = {'#FFFFFFFF', '#FFF1F1EE', '#FFB9DCF5', '#FFF7C1B5', '#FFC5E8CF', '#FFD9D4F2'}

function Traffic:init(city, count, seed)
    self.city = city
    self.random = Random(seed)
    self.cars = {}
    for index = 1, count do
        local car = {color = self.random:pick(Traffic.colors)}
        self:send(car)
        car.ridden = self.random:number(0, car.length)
        self.cars[index] = car
    end
end

-- Gives a car a new route from where it stands, or from a random corner.
function Traffic:send(car)
    local random = self.random
    local fromColumn, fromRow = car.column or random:integer(0, 16), car.row or random:integer(0, 16)
    local toColumn, toRow = random:integer(0, 16), random:integer(0, 16)
    car.route = self.city:route(fromColumn, fromRow, toColumn, toRow)
    car.length = City.routeLength(car.route)
    car.ridden = 0
    car.column, car.row = toColumn, toRow == City.riverRow and toRow - 1 or toRow
    if car.length == 0 then
        car.column, car.row = nil, nil
        self:send(car)
    end
end

function Traffic:update(dt)
    for _, car in ipairs(self.cars) do
        car.ridden = car.ridden + Traffic.speed * dt
        if car.ridden >= car.length then
            self:send(car)
        end
        car.x, car.y, car.dx, car.dy = City.pointAlong(car.route, car.ridden)
    end
end

return Traffic
