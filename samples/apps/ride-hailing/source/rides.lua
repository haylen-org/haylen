-- The rides of the person: the kinds of ride, the trip in progress, the past trips, the wallet and the traffic of the city. It is an autoload, so a trip goes on through its stages on every screen: the app looks for a driver, the driver comes, waits at the pickup and rides the route to the destination. Every change of stage emits "tripChanged" and shows a notice.
local events = require('haylen.events')

local City = require('city')
local Places = require('data.places')
local Random = require('random')
local Traffic = require('traffic')

local Rides = {}

Rides.city = City(31)
Rides.traffic = Traffic(Rides.city, 36, 5)
-- Map units of the city that make a kilometer of the prices and the distances.
Rides.unitsPerKilometer = 900
Rides.speed = 150

Rides.kinds = {
    {id = 'bike', name = 'Bike', art = 'rides/bike.svg', seats = 1, base = 1.2, perKilometer = 0.55, minutes = 2, note = 'Quick through traffic'},
    {id = 'compact', name = 'Compact', art = 'rides/compact.svg', seats = 4, base = 2.5, perKilometer = 0.95, minutes = 3, note = 'Affordable everyday rides'},
    {id = 'electric', name = 'Electric', art = 'rides/electric.svg', seats = 4, base = 2.8, perKilometer = 1.0, minutes = 5, note = 'Quiet electric cars'},
    {id = 'roomy', name = 'Roomy', art = 'rides/roomy.svg', seats = 4, base = 3.5, perKilometer = 1.25, minutes = 4, note = 'Newer cars with extra legroom'},
    {id = 'van', name = 'Van', art = 'rides/van.svg', seats = 6, base = 4.5, perKilometer = 1.6, minutes = 7, note = 'Room for groups and luggage'},
    {id = 'premium', name = 'Premium', art = 'rides/premium.svg', seats = 4, base = 7, perKilometer = 2.2, minutes = 6, note = 'Top rated drivers in black cars'},
}

Rides.stages = {
    {id = 'searching', title = 'Finding your driver', message = 'We are asking the drivers near you.', seconds = 5},
    {id = 'coming', title = 'Your driver is on the way', message = 'Meet your driver at the pickup point.'},
    {id = 'waiting', title = 'Your driver is here', message = 'Look for the car with the plate below.', seconds = 5},
    {id = 'riding', title = 'On your way', message = 'Enjoy the ride.'},
    {id = 'arrived', title = 'You arrived', message = 'Rate your ride and add a tip.'},
}

Rides.drivers = {
    {name = 'Marco Lindqvist', car = 'Teal compact', plate = 'RH 4821', rating = '4.92'},
    {name = 'Aiko Moreau', car = 'Silver sedan', plate = 'RH 1177', rating = '4.88'},
    {name = 'Samuel Okoye', car = 'Green hatchback', plate = 'EV 3090', rating = '4.97'},
    {name = 'Lucia Ferraz', car = 'Black sedan', plate = 'RH 7004', rating = '4.95'},
}

Rides.wallet = {balance = 32.5, cards = {
    {id = 'card', text = 'Card ending in 4821', caption = 'Expires 08 of 2029', icon = 'card'},
    {id = 'business', text = 'Business card ending in 1190', caption = 'Expires 11 of 2027', icon = 'card'},
}, payment = 'wallet', transactions = {}}

Rides.months = {'January', 'February', 'March', 'April', 'May', 'June', 'July', 'August', 'September', 'October', 'November', 'December'}

function Rides.money(amount)
    return string.format('$%.2f', amount)
end

function Rides.thousands(number)
    local text = tostring(number):reverse():gsub('(%d%d%d)', '%1,'):reverse():gsub('^,', '')
    return text
end

function Rides.kind(id)
    for _, kind in ipairs(Rides.kinds) do
        if kind.id == id then
            return kind
        end
    end
end

function Rides.price(kind, length)
    return kind.base + kind.perKilometer * length / Rides.unitsPerKilometer
end

function Rides.distanceText(length)
    return string.format('%.1f km', length / Rides.unitsPerKilometer)
end

function Rides.minutes(length)
    return math.max(2, math.floor(length / Rides.unitsPerKilometer * 3 + 0.5))
end

-- The route between two places and its length.
function Rides.route(from, to)
    local points = Rides.city:route(from.corner[1], from.corner[2], to.corner[1], to.corner[2])
    return points, City.routeLength(points)
end

function Rides:start()
    local random = Random(19)
    self.past = {}
    for index = 1, 520 do
        local month = 10 - (index - 1) // 48
        local from, to = Places.list[random:integer(1, #Places.list)], Places.list[random:integer(1, #Places.list)]
        local kind = random:pick(Rides.kinds)
        local length = random:number(1200, 9000)
        local cancelled = random:chance(0.05)
        self.past[#self.past + 1] = {
            id = 'trip-' .. index,
            type = 'trip',
            from = from.id,
            to = to.id,
            kind = kind.id,
            length = length,
            title = to.name,
            caption = string.format('%s %d · %s · %s', Rides.months[(month - 1) % 12 + 1], random:integer(1, 28), kind.name, Rides.distanceText(length)),
            price = cancelled and 'Cancelled' or Rides.money(Rides.price(kind, length)),
            fare = Rides.price(kind, length),
            cancelled = cancelled,
            month = Rides.months[(month - 1) % 12 + 1],
            driver = random:pick(Rides.drivers),
            art = kind.art,
        }
    end
    for index = 1, 12 do
        local trip = self.past[index]
        Rides.wallet.transactions[index] = {id = 'move-' .. index, type = 'move', icon = 'car', title = 'Ride to ' .. trip.title, caption = trip.caption, amount = trip.cancelled and '$0.00' or '-' .. trip.price}
    end
end

-- Starts a trip of a kind between two places and returns it.
function Rides.request(from, to, kindId, payment)
    local kind = Rides.kind(kindId)
    local route, length = Rides.route(from, to)
    local driver = Rides.drivers[(#Rides.past % #Rides.drivers) + 1]
    local approachFrom = {corner = {math.min(16, from.corner[1] + 3), math.max(0, from.corner[2] - 3)}}
    local approach, approachLength = Rides.route(approachFrom, from)
    local trip = {
        id = 'trip-now',
        type = 'trip',
        from = from.id,
        to = to.id,
        kind = kind.id,
        art = kind.art,
        route = route,
        length = length,
        approach = approach,
        approachLength = approachLength,
        fare = Rides.price(kind, length),
        payment = payment,
        driver = driver,
        stage = 1,
        elapsed = 0,
        title = to.name,
    }
    Rides.current = trip
    events.emit('tripChanged', trip)
    return trip
end

-- Seconds from the start of the trip to the start of a stage.
function Rides.stageStart(trip, stage)
    local start = 0
    for index = 1, stage - 1 do
        local seconds = Rides.stages[index].seconds
        if index == 2 then
            seconds = trip.approachLength / Rides.speed
        elseif index == 4 then
            seconds = trip.length / Rides.speed
        end
        start = start + seconds
    end
    return start
end

-- Where the car of the driver is: on the way to the pickup, at the pickup or on the route.
function Rides.carPosition(trip)
    if trip.stage == 2 then
        return City.pointAlong(trip.approach, (trip.elapsed - Rides.stageStart(trip, 2)) * Rides.speed)
    elseif trip.stage == 3 then
        return trip.route[1], trip.route[2], 0, 0
    end
    return City.pointAlong(trip.route, math.min(trip.length, (trip.elapsed - Rides.stageStart(trip, 4)) * Rides.speed))
end

-- Minutes the trip still takes, counting a minute of the ride as two seconds of the app.
function Rides.minutesLeft(trip)
    local left = Rides.stageStart(trip, 5) - trip.elapsed
    return math.max(1, math.ceil(left / 2))
end

function Rides.cancel()
    local trip = Rides.current
    Rides.current = nil
    events.emit('tripChanged', trip)
end

-- Ends a trip that arrived with its rating and tip, keeps it in the activity and pays it.
function Rides.finish(stars, tip)
    local trip = Rides.current
    local from, to = Places.find(trip.from), Places.find(trip.to)
    local kind = Rides.kind(trip.kind)
    local total = trip.fare + tip
    table.insert(Rides.past, 1, {
        id = 'trip-' .. (#Rides.past + 1),
        type = 'trip',
        from = from.id,
        to = to.id,
        kind = kind.id,
        length = trip.length,
        title = to.name,
        caption = string.format('Today · %s · %s', kind.name, Rides.distanceText(trip.length)),
        price = Rides.money(total),
        fare = total,
        month = 'Today',
        driver = trip.driver,
        art = kind.art,
        stars = stars,
    })
    if trip.payment == 'wallet' then
        Rides.wallet.balance = Rides.wallet.balance - total
    end
    table.insert(Rides.wallet.transactions, 1, {id = 'move-now-' .. #Rides.past, type = 'move', icon = 'car', title = 'Ride to ' .. to.name, caption = 'Today · ' .. kind.name, amount = '-' .. Rides.money(total)})
    Rides.current = nil
    events.emit('tripChanged', trip)
end

-- Adds money to the wallet.
function Rides.addFunds(amount)
    Rides.wallet.balance = Rides.wallet.balance + amount
    table.insert(Rides.wallet.transactions, 1, {id = 'move-funds-' .. #Rides.wallet.transactions, type = 'move', icon = 'plus', title = 'Funds added', caption = 'Today · Card ending in 4821', amount = '+' .. Rides.money(amount)})
    events.emit('walletChanged')
end

function Rides:update(dt)
    Rides.traffic:update(dt)
    local trip = Rides.current
    if not trip or trip.stage == #Rides.stages then
        return
    end
    trip.elapsed = trip.elapsed + dt
    local stage = trip.stage
    while stage < #Rides.stages and trip.elapsed >= Rides.stageStart(trip, stage + 1) do
        stage = stage + 1
    end
    if stage ~= trip.stage then
        trip.stage = stage
        events.emit('tripChanged', trip)
        events.emit('notice', Rides.stages[stage].title .. '.')
    end
end

return Rides
