-- Music Streaming: a made-up library of songs that the app makes with its own synthesizer, with home shelves, search, genres, a library of playlists, albums and artists, a player that follows every screen, the now playing screen with a visualizer, seeking, shuffle, repeat and a sleep timer, and a queue to reorder, in a dark and immersive look lit by the colors of the music.
local events = require('haylen.events')
local haylen = require('haylen')
local system = require('haylen.system')
local ui = require('haylen.ui')

local Style = require('style')

Style.register()

local Covers = require('covers')
local Shell = require('shell')
local Tour = require('tour')

-- The interface keeps its size in points on every screen, and a television, seen from the couch, draws it half as large again.
if system.info().deviceKind == 'tv' then
    ui.setScale(1.5)
end
ui.setTheme('music')
Covers.register()

events.on('notice', function(text)
    Shell.notify(text, 'information')
end)

Shell.start()

if haylen.platform == 'headless' then
    Tour.start()
end
