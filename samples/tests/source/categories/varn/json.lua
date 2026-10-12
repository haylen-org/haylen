-- JSON with Varn's "json" module: tables to text and back, with the way Lua values map to JSON values and back, the sentinel that keeps nulls, the marks of arrays and objects, and the errors that name the line or the key.
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
print(save.pos[2], save.boss)]]},
    {'Null, arrays and objects', [[
local loot = json.decode('{"boss": null, "drops": [1, null, 3]}',
    {nullValue = json.null})
print(loot.boss == json.null, #loot.drops)
print(json.encode(loot))
print(json.encode(json.array({})), json.encode(json.object({7})))
print(json.encode(json.decode('[]')))]]},
    {'Errors', [[
local ok, failure = pcall(json.decode, '{\n  "hp": 12,\n  "pos": [3 4]\n}')
local hero = {name = 'Ana', greet = print}
local ok, unsupported = pcall(json.encode, hero)
local node = {name = 'root'}
node.self = node
local ok, cycle = pcall(json.encode, node)]]},
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
    self:check('pretty', 'Pretty text', pretty:find('\n  "hp": 12', 1, true) ~= nil, 'Indented by two spaces, with the keys in order:\n' .. pretty)

    local save = json.decode('{"hp": 12, "speed": 1.5, '
        .. '"pos": [3, 4], "boss": null}')
    self:check('decode', 'Decode', math.type(save.hp) == 'integer' and math.type(save.speed) == 'float' and save.pos[2] == 4 and save.boss == nil, string.format('The number "hp" is an %s, "speed" a %s, "pos[2]" is %d, and "boss", which was null, is %s.', math.type(save.hp), math.type(save.speed), save.pos[2], tostring(save.boss)))

    local empty, holes = json.encode({}), json.encode({1, nil, 3})
    self:check('shapes', 'Arrays and objects', empty == '{}' and json.encode({1, 2}) == '[1,2]' and json.decode(holes)['3'] == 3, string.format('A sequence is an array, such as [1,2], while {} gave %s and a table with a hole gave the object %s.', empty, holes))

    local special = json.encode({1, 0 / 0, 1 / 0})
    self:check('numbers', 'Numbers JSON lacks', special == '[1,null,null]', string.format('Not a number and infinity have no JSON form, so they became null: %s.', special))

    self:nulls()
    self:markers()
    self:errors()

    self:check('aliases', 'Aliases', json.stringify({1}) == '[1]' and json.parse('[2]')[1] == 2, 'The functions "json.stringify" and "json.parse" are the same as "json.encode" and "json.decode".')
end

-- The sentinel "json.null" keeps the nulls that "nil" would drop, so an object keeps its keys and an array its length.
function Json:nulls()
    local loot = json.decode('{"boss": null, "drops": [1, null, 3]}',
        {nullValue = json.null})
    local again = json.encode(loot.drops)
    self:check('null', 'Null', loot.boss == json.null and #loot.drops == 3 and loot.drops[2] == json.null and again == '[1,null,3]', string.format('With "nullValue", "boss" is "json.null", "drops" keeps its %d items and encodes back as %s.', #loot.drops, again))
end

-- A decoded array remembers that it is one, and "json.array" and "json.object" say what an empty or a sparse table means.
function Json:markers()
    local array, object, decoded = json.encode(json.array({})), json.encode(json.object({7})), json.encode(json.decode('[]'))
    self:check('markers', 'Marked tables', array == '[]' and object == '{"1":7}' and decoded == '[]', string.format('An empty "json.array" gave %s, "json.object" of a sequence gave %s, and a decoded empty array encoded back as %s.', array, object, decoded))
end

-- Errors say where the text broke and which key holds a value that JSON cannot carry.
function Json:errors()
    local _, failure = pcall(json.decode, '{\n  "hp": 12,\n  "pos": [3 4]\n}')
    self:check('invalid', 'Invalid text', tostring(failure):find('line 3, column', 1, true) ~= nil, 'The decode raised, and "pcall" caught it: ' .. tostring(failure))

    local hero = {name = 'Ana', greet = print}
    local _, unsupported = pcall(json.encode, hero)
    local node = {name = 'root'}
    node.self = node
    local _, cycle = pcall(json.encode, node)
    self:check('unsupported', 'Values JSON cannot carry', tostring(unsupported):find('"greet"', 1, true) ~= nil and tostring(cycle):find('"self"', 1, true) ~= nil, string.format('A function raised "%s", and a table that holds itself raised "%s".', tostring(unsupported), tostring(cycle)))
end

return Json
