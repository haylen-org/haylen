-- The listings the person saved with the heart, newest first. Every change emits "savedChanged" on the event bus.
local events = require('haylen.events')

local Saved = {}

Saved.list = {}
Saved.ids = {}

function Saved.has(listing)
    return Saved.ids[listing.id] == true
end

function Saved.toggle(listing)
    if Saved.has(listing) then
        Saved.ids[listing.id] = nil
        for index, other in ipairs(Saved.list) do
            if other == listing then
                table.remove(Saved.list, index)
                break
            end
        end
    else
        Saved.ids[listing.id] = true
        table.insert(Saved.list, 1, listing)
    end
    events.emit('savedChanged', listing)
    return Saved.has(listing)
end

return Saved
