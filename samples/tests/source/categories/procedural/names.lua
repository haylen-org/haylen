-- Names made of syllables from a random generator, so the same seed always names the same towns and people.
local names = {}

names.onsets = {'', 'b', 'd', 'f', 'g', 'h', 'j', 'k', 'l', 'm', 'n', 'p', 'r', 's', 't', 'v', 'z', 'br', 'dr', 'kr', 'st', 'th'}
names.vowels = {'a', 'e', 'i', 'o', 'u', 'a', 'e', 'o', 'ia', 'ei'}
names.codas = {'n', 'r', 'l', 's', 'm', 'th'}
names.townEndings = {'ford', 'ton', 'by', 'wick', 'mere', 'holm', 'stead', 'port', 'dale', 'gate'}

function names.pick(random, list)
    return list[random:integer(1, #list)]
end

-- Returns a word of `syllables` syllables with its first letter in capitals, which ends in a consonant half of the time.
function names.word(random, syllables)
    local parts = {}
    for index = 1, syllables do
        local onset = names.pick(random, names.onsets)
        parts[#parts + 1] = (index > 1 and onset == '' and 'l' or onset) .. names.pick(random, names.vowels)
    end
    if random:chance(0.5) then
        parts[#parts + 1] = names.pick(random, names.codas)
    end
    local word = table.concat(parts)
    return word:sub(1, 1):upper() .. word:sub(2)
end

function names.person(random)
    return names.word(random, random:chance(0.7) and 2 or 3)
end

function names.town(random)
    return names.word(random, random:integer(1, 2)) .. names.pick(random, names.townEndings)
end

return names
