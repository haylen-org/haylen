-- A small seeded random generator, so the fake data of the app is the same on every run and every platform.
local haylen = require('haylen')

local Random = haylen.class('Random')

function Random:init(seed)
    self.state = (seed * 2654435761 + 1013904223) % 4294967296
end

-- Returns a number from 0 up to 1.
function Random:next()
    self.state = (self.state * 1664525 + 1013904223) % 4294967296
    return self.state / 4294967296
end

-- Returns a whole number from `low` to `high`.
function Random:integer(low, high)
    return low + math.floor(self:next() * (high - low + 1))
end

function Random:number(low, high)
    return low + self:next() * (high - low)
end

function Random:pick(list)
    return list[self:integer(1, #list)]
end

function Random:chance(probability)
    return self:next() < probability
end

return Random
