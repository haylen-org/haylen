-- JSON with Varn's "json" module: tables to text and back, with the way Lua values map to JSON values and back.
local haylen = require('haylen')
local json = require('json')

local VarnTest = require('categories.varn.varn-test')

local Json = haylen.class('Json', VarnTest)

Json.excerpts = {
    {'Encode', [[
local text = json.encode({
    name = 'Ana', level = 3, items = {'rope', 'lamp'},
})
local pretty = json.encode({hp = 12, pos = {3, 4}}, {
    pretty = true,
})
print(json.encode({}), json.encode({1, nil, 3}))
print(json.encode({1, 0 / 0, 1 / 0}))]]},
    {'Decode', [[
local save = json.decode('{"hp": 12, "speed": 1.5, '
    .. '"pos": [3, 4], "boss": null}')
print(math.type(save.hp), math.type(save.speed))
print(save.pos[2], save.boss)

local ok, failure = pcall(json.decode, '{oops}')]]},
}

function Json:run()
    local text = json.encode({
        name = 'Ana', level = 3, items = {'rope', 'lamp'},
    })
    local back = json.decode(text)
    self:check('encode', 'Encode', back.name == 'Ana' and back.level == 3 and back.items[2] == 'lamp', string.format('Wrote %s. Lua tables keep no order, so the keys of an object come in any order.', text))

    local pretty = json.encode({hp = 12, pos = {3, 4}}, {
        pretty = true,
    })
    self:check('pretty', 'Pretty text', pretty:find('\n  "hp": 12', 1, true) ~= nil, 'Indented by two spaces:\n' .. pretty)

    local save = json.decode('{"hp": 12, "speed": 1.5, '
        .. '"pos": [3, 4], "boss": null}')
    self:check('decode', 'Decode', math.type(save.hp) == 'integer' and math.type(save.speed) == 'float' and save.pos[2] == 4 and save.boss == nil, string.format('The number "hp" is an %s, "speed" a %s, "pos[2]" is %d, and "boss", which was null, is %s.', math.type(save.hp), math.type(save.speed), save.pos[2], tostring(save.boss)))

    local empty, holes = json.encode({}), json.encode({1, nil, 3})
    self:check('shapes', 'Arrays and objects', empty == '{}' and json.encode({1, 2}) == '[1,2]' and json.decode(holes)['3'] == 3, string.format('A sequence is an array, such as [1,2], while {} gave %s and a table with a hole gave the object %s.', empty, holes))

    local special = json.encode({1, 0 / 0, 1 / 0})
    self:check('numbers', 'Numbers JSON lacks', special == '[1,null,null]', string.format('Not a number and infinity have no JSON form, so they became null: %s.', special))

    local ok, failure = pcall(json.decode, '{oops}')
    self:check('invalid', 'Invalid text', not ok and failure ~= nil, 'The call raised, and "pcall" caught it: ' .. tostring(failure))

    self:check('aliases', 'Aliases', json.stringify({1}) == '[1]' and json.parse('[2]')[1] == 2, 'The functions "json.stringify" and "json.parse" are the same as "json.encode" and "json.decode".')
end

return Json
