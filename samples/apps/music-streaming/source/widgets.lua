-- The building blocks the screens share: the templates of the cells of songs, albums, artists and playlists, and what the menu of a song does.
local ui = require('haylen.ui')

local Catalog = require('data.catalog')
local Player = require('player')
local Shell = require('shell')

local Widgets = {}

Widgets.trackMenu = {
    {id = 'next', text = 'Play next', image = Shell.icon('next')},
    {id = 'queue', text = 'Add to the queue', image = Shell.icon('queue')},
    {id = 'playlist', text = 'Add to a playlist', image = Shell.icon('plus')},
    {id = 'artist', text = 'Go to the artist', image = Shell.icon('music')},
    {id = 'album', text = 'Go to the album', image = Shell.icon('library')},
}

-- A row of a song with its cover or its number, its name, its artist and its length, across `span` lanes of a grid. A right click, a long press or the menu button open the menu of the song.
function Widgets.trackRow(showCover, span)
    return {template = ui.contextMenu{part = 'menu', items = Widgets.trackMenu,
        ui.row{gap = 14, padding = {6, 8},
            ui.label{part = 'number', bind = {text = '$index'}, width = 44, font = 'caption', color = 'textMuted', textAlign = 'end', visible = not showCover},
            ui.image{part = 'cover', bind = {image = 'cover'}, image = Shell.blankCover, width = 52, height = 52, radius = 8, visible = showCover == true},
            ui.column{gap = 0, grow = 1,
                ui.label{part = 'name', bind = {text = 'name'}, font = 'button', wrap = false},
                ui.label{part = 'artist', bind = {text = 'caption'}, font = 'caption', color = 'textMuted', wrap = false},
            },
            ui.label{part = 'duration', bind = {text = 'durationText'}, font = 'caption', color = 'textMuted'},
            ui.icon{image = Shell.icon('more'), size = 24, color = 'textMuted'},
        },
    }, interactive = true, span = span, estimatedSize = 68}
end

-- A square card of an album, a mix or a playlist in the shelves and grids.
function Widgets.coverCard(width)
    return {template = ui.column{gap = 6, width = width, padding = {0, 0, 8, 0},
        ui.image{part = 'cover', bind = {image = 'cover'}, image = Shell.blankCover, align = 'stretch', aspectRatio = 1, radius = 14},
        ui.label{part = 'name', bind = {text = 'name'}, font = 'button', wrap = false},
        ui.label{part = 'caption', bind = {text = 'caption'}, font = 'caption', color = 'textMuted', wrap = false},
    }}
end

-- A round card of an artist.
function Widgets.artistCard(width)
    return {template = ui.column{gap = 6, width = width, padding = {0, 0, 8, 0},
        ui.image{part = 'cover', bind = {image = 'cover'}, image = Shell.blankCover, width = width, height = width, radius = width / 2},
        ui.label{part = 'name', bind = {text = 'name'}, font = 'button', textAlign = 'center', align = 'stretch', wrap = false},
    }}
end

-- What a page does for the events of a collection of songs: a press plays the list from that song, and the menu acts on the song.
function Widgets.onTracks(tracks, page)
    return function(event)
        local track = Catalog.find(event.part == 'menu' and event.cell.item or event.item)
        if event.part ~= 'menu' then
            for index, other in ipairs(tracks()) do
                if other == track then
                    Player.playList(tracks(), index)
                    return
                end
            end
            return
        end
        if event.item == 'next' then
            Player.addToQueue(track, true)
            Shell.notify(track.name .. ' plays next.')
        elseif event.item == 'queue' then
            Player.addToQueue(track)
            Shell.notify(track.name .. ' is in the queue.')
        elseif event.item == 'playlist' then
            Widgets.askPlaylist(page, track)
        elseif event.item == 'artist' then
            Shell.open(require('screens.artist'), {artist = track.artist})
        elseif event.item == 'album' then
            Shell.open(require('screens.collection'), {collection = track.album})
        end
    end
end

-- Opens the dialog of `Widgets.playlistDialog` for a song.
function Widgets.askPlaylist(page, track)
    page.pendingTrack = track
    page.gui:set('playlistDialog', {open = true, message = 'Add ' .. track.name .. ' to a playlist.'})
end

-- The dialog that adds a song to one of the playlists, which every page that shows songs mounts.
function Widgets.playlistDialog(page)
    local items = {}
    for _, playlist in ipairs(Catalog.playlists) do
        if playlist.id ~= 'liked' then
            items[#items + 1] = {id = playlist.id, text = playlist.name}
        end
    end
    page.chosenPlaylist = items[1].id
    return ui.dialog{id = 'playlistDialog', title = 'Add to a playlist', message = '', buttons = {{id = 'cancel', text = 'Cancel'}, {id = 'add', text = 'Add', variant = 'primary'}},
        onAnswer = function(event)
            if event.button == 'add' then
                local playlist = Catalog.find(page.chosenPlaylist)
                table.insert(playlist.tracks, 1, page.pendingTrack)
                playlist.caption = string.format('Playlist · %s songs', Catalog.thousands(#playlist.tracks))
                Shell.notify(page.pendingTrack.name .. ' is in ' .. playlist.name .. '.')
            end
        end,
        ui.radioGroup{id = 'playlistChoice', items = items, selected = items[1].id, onChange = function(event)
            page.chosenPlaylist = event.value
        end},
    }
end

return Widgets
