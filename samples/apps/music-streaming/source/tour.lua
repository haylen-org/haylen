-- The automatic run of the headless platform: it plays a mix, opens the player, seeks, picks a shorter song, skips, shuffles and repeats, sets the sleep timer, shares the song, pulls the queue up, reorders it and removes a song, browses the favorites, an artist, the search, a genre and the library, adds a song to a playlist and takes one out, makes a playlist and deletes it after the alert, opens links of the scheme and of the web, logs each step and quits, so an unattended run checks every path of the app.
local haylen = require('haylen')
local log = require('haylen.log')
local timer = require('haylen.timer')
local ui = require('haylen.ui')

local Catalog = require('data.catalog')
local Player = require('player')
local Routes = require('routes')
local Shell = require('shell')
local Widgets = require('widgets')

local Tour = {}

Tour.interval = 1.5

function Tour.steps()
    local linkedSong = Catalog.albums[30].tracks[2]
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
        {'Share the song', function() Shell.top():share() end},
        {'Copy the link of the app', function() Shell.top().shareSheet:dismiss({result = 'app'}) end},
        {'Open the queue at half the screen', function() Shell.openQueue() end},
        {'Pull the queue up', function() Shell.queue.presentation.detent = 2 end},
        {'Reorder the queue', function() Player.moveInQueue(Player.index + 3, Player.index + 1) end},
        {'Remove a song from the queue', function() Shell.queue:drop(Player.index + 2) end},
        {'Close the queue and the player', function() ui.goBack() ui.goBack() end},
        {'Open the favorites', function() Shell.open(Routes.location('liked')) end},
        {'Scroll the favorites', function() Shell.top().gui:collection('tracks'):scrollTo(2000, {align = 'center', animated = false}) end},
        {'Open an artist', function() Shell.open(Routes.location(Catalog.artists[4].id)) end},
        {'Follow the artist', function() local page = Shell.top() require('screens.artist').following[page.artist.id] = true page:showFollow() end},
        {'Add a song to a playlist', function() local page = Shell.top() Widgets.askPlaylist(page, page.artist.tracks[2]) end},
        {'Open a playlist', function() Shell.top().playlistSheet:dismiss({result = 'drive'}) Shell.open(Routes.location('drive')) end},
        {'Take a song out of the playlist', function() local page = Shell.top() page:removeTrack(page.source.tracks[1]) end},
        {'Search', function() Shell.showTab('search') end},
        {'Type a search', function() local page = Shell.top() page.query = 'golden' page.gui:set('query', {value = 'golden'}) page:refresh() end},
        {'Open a genre', function() Shell.open('genre/jazz') end},
        {'Open the library', function() Shell.showTab('library') end},
        {'Show the albums', function() Shell.top():pick('albums') end},
        {'Make a playlist', function() local page = Shell.top() page:pick('playlists') page:create('Road trip') end},
        {'Open the new playlist', function() Shell.open(Routes.location(Catalog.playlists[2].id)) end},
        {'Ask before deleting it', function() Shell.top():askDelete() end},
        {'Delete it', function() Shell.top().deleteAlert:dismiss({result = 'confirm'}) end},
        {'Back home, which kept its screens', function() Shell.showTab('home') end},
        {'Back to the root of home', function() Shell.showTab('home') end},
        {'Open a link to an artist', function() Shell.router:open(Routes.link('artist/12')) end},
        {'Open a web link to a song of an album', function() Shell.router:open(Routes.webLink(Routes.location(linkedSong.id))) end},
        {'Open a link to a search', function() Shell.router:open(Routes.link('search/golden')) end},
        {'Open a link to an artist the library lacks', function() Shell.router:open(Routes.link('artist/9999')) end},
        {'Open the queue from a link', function() Shell.router:open(Routes.link('queue')) end},
        {'Close the queue', function() ui.goBack() end},
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
