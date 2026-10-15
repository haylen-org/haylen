-- The automatic run of the headless platform: it plays a mix, opens the player, seeks, skips, shuffles and repeats, sets the sleep timer, reorders the queue, browses the liked songs, an artist, the search, a genre and the library, adds a song to a playlist and switches the theme, logs each step and quits, so an unattended run checks every path of the app.
local haylen = require('haylen')
local log = require('haylen.log')
local scene = require('haylen.scene')
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
        {'Skip to the next song', function() Player.next() end},
        {'Shuffle and repeat', function() Player.setShuffle(true) Player.cycleRepeat() end},
        {'Set the sleep timer', function() scene.top():setSleep('15') end},
        {'Like the song', function() scene.top():toggleLike() end},
        {'Open the queue', function() Shell.open(require('screens.queue')) end},
        {'Reorder the queue', function() Player.moveInQueue(Player.index + 3, Player.index + 1) Player.removeFromQueue(Player.index + 2) end},
        {'Close the player', function() Shell.back() Shell.back() end},
        {'Open the liked songs', function() Shell.open(require('screens.collection'), {collection = 'liked'}) end},
        {'Scroll the liked songs', function() scene.top().gui:collection('tracks'):scrollTo(2000, {align = 'center', animated = false}) end},
        {'Open an artist', function() Shell.open(require('screens.artist'), {artist = Catalog.artists[4].id}) end},
        {'Follow the artist', function() local page = scene.top() require('screens.artist').following[page.artist.id] = true page:showFollow() end},
        {'Add a song to a playlist', function() local page = scene.top() Widgets.askPlaylist(page, page.artist.tracks[2]) end},
        {'Search', function() scene.top().gui:set('playlistDialog', {open = false}) Shell.showTab('search') end},
        {'Type a search', function() local page = scene.top() page.query = 'golden' page.gui:set('query', {value = 'golden'}) page:refresh() end},
        {'Open a genre', function() Shell.open(require('screens.collection'), {collection = 'genre-jazz'}) end},
        {'Open the library', function() Shell.showTab('library') end},
        {'Show the albums', function() local page = scene.top() page.section = 'albums' page.gui:set('section', {selected = 'albums'}) page:show() end},
        {'Make a playlist', function() local page = scene.top() page.newName = 'Road trip' page:create() end},
        {'Turn on the light theme', function() ui.setTheme('musicDay') Shell.showTab('home') end},
        {'Player in the light theme', function() Shell.openPlayer() end},
        {'Back to the night', function() ui.setTheme('musicNight') Shell.showTab('home') end},
    }
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
