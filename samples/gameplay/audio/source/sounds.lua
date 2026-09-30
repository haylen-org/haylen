-- The sounds of the sample: short effects decoded into memory and music streamed from its file, all in the audio preload group that every test waits for.
local assets = require('haylen.assets')

local sounds = {}

sounds.effects = {
    {id = 'chop', text = 'Chop', path = 'audio/effects/chop.ogg'},
    {id = 'hit', text = 'Hit', path = 'audio/effects/hit_1.ogg'},
    {id = 'footstep', text = 'Footstep', path = 'audio/effects/footstep_1.ogg'},
    {id = 'swing', text = 'Swing', path = 'audio/effects/swing_1.wav'},
    {id = 'pickup', text = 'Pickup', path = 'audio/effects/wood_pickup.ogg'},
    {id = 'explosion', text = 'Explosion', path = 'audio/effects/explosion.ogg'},
}

sounds.tracks = {
    {id = 'inn', text = 'The Old Tower Inn', path = 'audio/music/old_tower_inn.mp3'},
    {id = 'market', text = 'Market Day', path = 'audio/music/market_day.mp3'},
}

sounds.kStream = {stream = true}

function sounds.defineGroup()
    assets.defineGroups({groups = {audio = {
        'audio/effects/',
        'audio/ui/',
        'audio/ambient/',
        'audio/generated/',
        {path = 'audio/music/', options = sounds.kStream},
    }}})
end

-- Returns a decoded sound, such as `audio/effects/chop.ogg`.
function sounds.get(path)
    return assets.load(path)
end

-- Returns a music track, streamed with the options the preload group used, so the cached stream is reused.
function sounds.track(path)
    return assets.load(path, 'sound', sounds.kStream)
end

-- Finds the entry of a list by its id.
function sounds.find(list, id)
    for _, entry in ipairs(list) do
        if entry.id == id then
            return entry
        end
    end
end

-- Returns the items of a list for the choice components of the UI, which take only an id and a text.
function sounds.items(list)
    local items = {}
    for index, entry in ipairs(list) do
        items[index] = {id = entry.id, text = entry.text}
    end
    return items
end

return sounds
