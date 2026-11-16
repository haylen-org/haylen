-- The routes of the app: the location of every screen that a link opens, such as "artist/12" or "album/30?track=4". The links of the scheme of the app, "haylenmusic://artist/12", and of its web host, "https://music.example.com/artist/12", open them from a cold start and while the app runs, and the screens that the person opens by hand go through the same routes, so the router knows where every screen leads and builds the navigation again after the system ended the app.
local haylen = require('haylen')
local ui = require('haylen.ui')

local Catalog = require('data.catalog')
local Page = require('page')
local Shell = require('shell')
local Style = require('style')

local Routes = {}

Routes.scheme = 'haylenmusic'
Routes.host = 'music.example.com'

-- What a link to something the library does not hold shows, with the way back.
local Missing = haylen.class('Missing', Page)

function Missing:tree()
    return ui.column{grow = 1, gap = 0,
        Shell.header(''),
        ui.emptyState{grow = 1, image = Shell.icon('music'), imageSize = 72, title = 'Nothing here', message = self.params.text},
    }
end

-- Creates the router over the tabs of the app, which lives as long as its owner.
function Routes.start(tabs, owner)
    return ui.router{tabs = tabs, schemes = {Routes.scheme}, hosts = {Routes.host}, restore = true, owner = owner, routes = {
        {name = 'home', path = 'home', tab = 'home'},
        {name = 'search', path = 'search', tab = 'search'},
        {name = 'results', path = 'search/{query}', tab = 'search', screen = Routes.resultsScreen},
        {name = 'library', path = 'library', tab = 'library'},
        {name = 'artist', path = 'artist/{id:integer}', screen = Routes.artistScreen},
        {name = 'album', path = 'album/{id:integer}', query = {track = 'integer'}, screen = Routes.albumScreen},
        {name = 'playlist', path = 'playlist/{id}', screen = Routes.playlistScreen},
        {name = 'genre', path = 'genre/{id}', tab = 'search', screen = Routes.genreScreen},
        {name = 'player', path = 'player', present = {style = 'fullScreen'}, screen = Routes.playerScreen},
        {name = 'queue', path = 'queue', present = {detents = {'medium', 'large'}, size = 'page'}, screen = Routes.queueScreen},
    }}
end

-- The location of an artist, an album, a song, a playlist, a mix or a genre by the id the catalog gives it. A song leads to its album with its number in the query.
function Routes.location(id)
    local artist = id:match('^artist%-(%d+)$')
    if artist then
        return 'artist/' .. artist
    end
    local album = id:match('^album%-(%d+)$')
    if album then
        return 'album/' .. album
    end
    local track = id:match('^track%-(%d+)$')
    if track then
        return Routes.location(Catalog.find(id).album) .. '?track=' .. track
    end
    local genre = id:match('^genre%-(.+)$')
    if genre then
        return 'genre/' .. genre
    end
    return 'playlist/' .. id
end

function Routes.link(location)
    return Routes.scheme .. '://' .. location
end

function Routes.webLink(location)
    return 'https://' .. Routes.host .. '/' .. location
end

function Routes.resultsScreen(parameters)
    return require('screens.search')({query = parameters.query})
end

function Routes.artistScreen(parameters)
    local id = 'artist-' .. parameters.id
    if not Catalog.find(id) then
        return Missing({text = 'This artist is not in the library.'})
    end
    return require('screens.artist')({artist = id})
end

function Routes.albumScreen(parameters)
    local id = 'album-' .. parameters.id
    if not Catalog.find(id) then
        return Missing({text = 'This album is not in the library.'})
    end
    local track = parameters.track and Catalog.find('track-' .. parameters.track)
    return require('screens.collection')({collection = id, track = track and track.album == id and track.id or nil})
end

function Routes.playlistScreen(parameters)
    local found = Catalog.find(parameters.id)
    if not found or found.type ~= 'playlist' then
        return Missing({text = 'This playlist is not in your library.'})
    end
    return require('screens.collection')({collection = found.id})
end

function Routes.genreScreen(parameters)
    if not Catalog.genre(parameters.id) then
        return Missing({text = 'This genre is not in the library.'})
    end
    return require('screens.collection')({collection = 'genre-' .. parameters.id})
end

-- The player and the queue are presentations, which the shell keeps to know when they show.
function Routes.playerScreen()
    Shell.player = require('screens.now-playing')()
    return Shell.player
end

function Routes.queueScreen()
    Shell.queue = require('screens.queue')()
    return Shell.queue
end

return Routes
