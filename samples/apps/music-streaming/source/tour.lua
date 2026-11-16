-- The automatic run of the headless platform: it plays a mix, opens the player, seeks, picks a shorter song, skips, shuffles and repeats, sets the sleep timer, reorders the queue, browses the favorites, an artist, the search, a genre and the library, adds a song to a playlist and makes a playlist, logs each step and quits, so an unattended run checks every path of the app.
local haylen = require('haylen')
local log = require('haylen.log')
local timer = require('haylen.timer')
local ui = require('haylen.ui')

local Catalog = require('data.catalog')
local Player = require('player')
local Shell = require('shell')
local Widgets = require('widgets')

local Tour = {}

Tour.interval = 1.5

function Tour.steps()
    return {
        {'Home', function() end},
        {'Play a mix', function() Player.playList(Catalog.mixes[2].tracks, 1) end},
        {'Open the player', function() Shell.openPlayer() end},
        {'Seek into the song', function() Player.seek(Player.current().length / 2) end},
        {'Pick a shorter song near the end of this one', function() Tour.pickShorterSong() end},
        {'Skip to the next song', function() Player.next() end},
        {'Shuffle and repeat', function() Player.setShuffle(true) Player.cycleRepeat() end},
        {'Set the sleep timer', function() Shell.top():setSleep('15') end},
        {'Like the song', function() Catalog.toggleLike(Player.current()) end},
        {'Open the queue', function() Shell.openQueue() end},
        {'Reorder the queue', function() Player.moveInQueue(Player.index + 3, Player.index + 1) Player.removeFromQueue(Player.index + 2) end},
        {'Close the queue and the player', function() ui.goBack() ui.goBack() end},
        {'Open the favorites', function() Shell.open(require('screens.collection'), {collection = 'liked'}) end},
        {'Scroll the favorites', function() Shell.top().gui:collection('tracks'):scrollTo(2000, {align = 'center', animated = false}) end},
        {'Open an artist', function() Shell.open(require('screens.artist'), {artist = Catalog.artists[4].id}) end},
        {'Follow the artist', function() local page = Shell.top() require('screens.artist').following[page.artist.id] = true page:showFollow() end},
        {'Add a song to a playlist', function() local page = Shell.top() Widgets.askPlaylist(page, page.artist.tracks[2]) end},
        {'Search', function() Shell.top().playlistSheet:dismiss() Shell.showTab('search') end},
        {'Type a search', function() local page = Shell.top() page.query = 'golden' page.gui:set('query', {value = 'golden'}) page:refresh() end},
        {'Open a genre', function() Shell.open(require('screens.collection'), {collection = 'genre-jazz'}) end},
        {'Open the library', function() Shell.showTab('library') end},
        {'Show the albums', function() Shell.top():pick('albums') end},
        {'Make a playlist', function() local page = Shell.top() page:pick('playlists') page:create('Road trip') end},
        {'Back home, which kept its screens', function() Shell.showTab('home') end},
        {'Back to the root of home', function() Shell.showTab('home') end},
        {'Player again', function() Shell.openPlayer() end},
        {'Close it', function() ui.goBack() end},
    }
end

-- Plays a song shorter than the time the current song reached, as a player who taps another song does, and checks that the time of the old song never counts for the new one while the new one is made, which the bar of the player shows.
function Tour.pickShorterSong()
    local current = Player.current()
    local shortest
    for _, artist in ipairs(Catalog.artists) do
        for _, track in ipairs(artist.tracks) do
            if track.id ~= current.id and (not shortest or track.length < shortest.length) then
                shortest = track
            end
        end
    end
    Player.seek(math.min(current.length - 1, shortest.length + 5))
    Player.addToQueue(shortest, true)
    Player.next()
    if Player.elapsed() > Player.current().length then
        error('The time of the song before counts for the picked song while it is made.')
    end
end

function Tour.start()
    local steps = Tour.steps()
    local index = 0
    timer.every(Tour.interval, function()
        index = index + 1
        if index > #steps then
            log.info('The tour finished.')
            haylen.quit()
            return
        end
        log.info(string.format('Step %d of %d: %s.', index, #steps, steps[index][1]))
        steps[index][2]()
    end, {owner = Tour})
end

return Tour
