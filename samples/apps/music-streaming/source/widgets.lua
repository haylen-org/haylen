-- The building blocks the screens share: the templates of the cells of songs, covers and artists, the shelves, what the menu of a song does and the dialog that adds a song to a playlist.
local ui = require('haylen.ui')

local Catalog = require('data.catalog')
local Player = require('player')
local Shell = require('shell')
local Style = require('style')

local Widgets = {}

Widgets.trackMenu = {
    {id = 'next', text = 'Play next', image = Shell.icon('next')},
    {id = 'queue', text = 'Add to the queue', image = Shell.icon('queue')},
    {id = 'playlist', text = 'Add to a playlist', image = Shell.icon('plus')},
    {id = 'artist', text = 'Go to the artist', image = Shell.icon('user')},
    {id = 'album', text = 'Go to the album', image = Shell.icon('library')},
}

-- A row of a song with its cover or its number, its name, its artist and its length, across `span` lanes of a grid. A right click, a long press or the menu button open the menu of the song.
function Widgets.trackRow(showCover, span)
    local space = Style.space
    return {template = ui.contextMenu{part = 'menu', items = Widgets.trackMenu,
        ui.row{gap = space.m, padding = {space.xs, space.s},
            ui.label{part = 'number', bind = {text = '$index'}, width = 40, color = 'textMuted', textAlign = 'end', visible = not showCover},
            ui.image{part = 'cover', bind = {image = 'cover'}, image = Shell.blankCover, width = 60, height = 60, radius = Style.radius.cover, visible = showCover == true},
            ui.column{gap = 0, grow = 1,
                ui.label{part = 'name', bind = {text = 'name'}, wrap = false},
                ui.label{part = 'artist', bind = {text = 'caption'}, font = 'caption', color = 'textMuted', wrap = false},
            },
            ui.label{part = 'duration', bind = {text = 'durationText'}, font = 'caption', color = 'textMuted'},
            ui.icon{image = Shell.icon('more'), size = 26, color = 'textMuted'},
        },
    }, interactive = true, span = span, estimatedSize = 84}
end

-- A square card of an album, a mix or a playlist in the shelves and grids, the cover large over its name.
function Widgets.coverCard(width)
    local space = Style.space
    return {template = ui.column{gap = space.xxs, width = width, padding = {space.xs, space.xs, space.s, space.xs},
        ui.image{part = 'cover', bind = {image = 'cover'}, image = Shell.blankCover, align = 'stretch', aspectRatio = 1, radius = Style.radius.cover, margin = {0, 0, space.xs, 0}},
        ui.label{part = 'name', bind = {text = 'name'}, font = 'button', wrap = false},
        ui.label{part = 'caption', bind = {text = 'caption'}, font = 'caption', color = 'textMuted', wrap = false},
    }}
end

-- A round card of an artist.
function Widgets.artistCard(width)
    return {template = ui.column{gap = Style.space.xs, width = width, padding = Style.space.xs,
        ui.image{part = 'cover', bind = {image = 'cover'}, image = Shell.blankCover, align = 'stretch', aspectRatio = 1, radius = 400},
        ui.label{part = 'name', bind = {text = 'name'}, font = 'button', textAlign = 'center', align = 'stretch', wrap = false},
        ui.label{text = 'Artist', font = 'caption', color = 'textMuted', textAlign = 'center', align = 'stretch'},
    }}
end

-- A shelf of covers or artists that scrolls sideways and opens what is picked.
function Widgets.shelf(id, kind, width)
    local card = kind == 'artist' and Widgets.artistCard(width or Style.size.artist) or Widgets.coverCard(width or Style.size.cover)
    return ui.collection{id = id, axis = 'horizontal', scrollbar = false, gap = Style.space.xs, rememberFocus = true, types = {[kind] = card}, onSelect = function(event)
        Widgets.openItem(event.item)
    end}
end

-- A title of a section with room above it.
function Widgets.section(text)
    return ui.label{text = text, font = 'heading', margin = {Style.space.l, 0, 0, Style.space.xs}}
end

-- Opens an artist, an album, a playlist or a mix by its id.
function Widgets.openItem(id)
    local found = Catalog.find(id)
    if found.type == 'artist' then
        Shell.open(require('screens.artist'), {artist = found.id})
    else
        Shell.open(require('screens.collection'), {collection = found.id})
    end
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

-- The big round play button of a page, in the accent of the app, or in the color of the text with `light`.
function Widgets.playButton(id, size, properties, light)
    local style = {metrics = {controlRadius = size / 2, controlPaddingX = 0, iconSize = math.floor(size * 0.42)}, colors = light and Shell.lightColors or nil}
    local button = {id = id, icon = Shell.icon('play'), iconColor = 'onAccent', variant = 'primary', width = size, height = size, accessibilityLabel = 'Play', style = style}
    for key, value in pairs(properties or {}) do
        button[key] = value
    end
    return ui.button(button)
end

return Widgets
