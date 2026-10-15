-- The places of the made-up city a rider can go to: their names, kinds, streets and corners on the map. Every value comes from a seeded generator, so the same city shows on every run.
local Random = require('random')

local Places = {}

Places.count = 565
Places.kinds = {
    {'Café', {'Morning', 'Corner', 'Little', 'Blue Cup', 'Steam'}},
    {'Station', {'Central', 'North', 'Harbor', 'Riverside', 'Market'}},
    {'Park', {'Willow', 'Oak', 'Sunset', 'Lantern', 'Hillside'}},
    {'Museum', {'City', 'Maritime', 'Modern Art', 'Science', 'Clock'}},
    {'Library', {'Public', 'University', 'Garden', 'Old Town'}},
    {'Market', {'Farmers', 'Night', 'Fish', 'Flower', 'Covered'}},
    {'Hotel', {'Grand', 'Harbor View', 'Lantern', 'Riverside', 'Park'}},
    {'Clinic', {'Family', 'Riverside', 'Midtown', 'North End'}},
    {'Gym', {'Iron', 'Peak', 'Pulse', 'Core'}},
    {'Cinema', {'Starlight', 'Roxy', 'Palace', 'Velvet'}},
    {'Bakery', {'Golden', 'Rye', 'Crumb', 'Daily'}},
    {'Stadium', {'City', 'River', 'Arena'}},
}
Places.streets = {'Harbor Lane', 'Market Street', 'Garden Row', 'Maple Avenue', 'Lantern Way', 'River Road', 'Hill Street', 'Station Square', 'Copper Street', 'Willow Avenue', 'North Road', 'Bridge Street'}
-- The places the person saved, with the first places of the list as their corners.
Places.saved = {
    {id = 'home', text = 'Home', icon = 'home', corner = {3, 13}, line = '12 Harbor Lane'},
    {id = 'work', text = 'Work', icon = 'flag', corner = {12, 3}, line = '480 Market Street'},
}
Places.here = {id = 'here', name = 'Current location', line = '7 Garden Row', corner = {6, 11}}

function Places.build()
    local random = Random(53)
    Places.list = {}
    Places.byId = {}
    for index = 1, Places.count do
        local kind = random:pick(Places.kinds)
        local row = random:pick({0, 1, 2, 3, 4, 5, 6, 7, 8, 10, 11, 12, 13, 14, 15, 16})
        local place = {
            id = 'p' .. index,
            type = 'place',
            name = random:pick(kind[2]) .. ' ' .. kind[1],
            kind = kind[1],
            line = random:integer(1, 980) .. ' ' .. random:pick(Places.streets),
            corner = {random:integer(0, 16), row},
        }
        place.caption = place.kind .. ' · ' .. place.line
        place.searchText = string.lower(place.name .. ' ' .. place.line)
        Places.list[index] = place
        Places.byId[place.id] = place
    end
    for _, saved in ipairs(Places.saved) do
        local place = {id = saved.id, type = 'place', name = saved.text, kind = 'Saved place', line = saved.line, corner = saved.corner}
        place.caption = 'Saved place · ' .. saved.line
        place.searchText = string.lower(saved.text .. ' ' .. saved.line)
        Places.byId[saved.id] = place
    end
    Places.byId.here = Places.here
end

function Places.find(id)
    return Places.byId[id]
end

Places.build()

return Places
