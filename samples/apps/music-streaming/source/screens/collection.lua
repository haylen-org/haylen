-- An album, a playlist, a mix or a genre: the big cover, the names and the buttons that play and shuffle under the glow of its colors, over the songs in a recycled list, which holds all 2,400 liked songs.
local haylen = require('haylen')
local ui = require('haylen.ui')

local Catalog = require('data.catalog')
local Page = require('page')
local Player = require('player')
local Shell = require('shell')
local Style = require('style')
local Widgets = require('widgets')

local Collection = haylen.class('Collection', Page)

function Collection:enter()
    local source = self:source(self.params.collection)
    self.source = source
    self.glow = source.genre or (source.tracks[1] and source.tracks[1].genre)
    self.onTracks = Widgets.onTracks(function() return source.tracks end, self)
    self:mountGui(Shell.frame(Shell.current, ui.column{grow = 1, gap = 0,
        Shell.header(''),
        ui.column{id = 'body', grow = 1, gap = 0},
        Widgets.playlistDialog(self),
    }))
end

function Collection:layoutChanged(wide)
    local space = Style.space
    local source = self.source
    local cover = ui.image{part = 'cover', bind = {image = 'cover'}, image = Shell.blankCover, width = wide and 232 or 260, height = wide and 232 or 260, radius = Style.radius.cover, align = wide and 'start' or 'center'}
    local texts = ui.column{gap = space.xxs, grow = 1, minWidth = 260,
        ui.label{part = 'kind', bind = {text = 'kind'}, font = 'caption', color = 'textMuted'},
        ui.label{part = 'name', bind = {text = 'name'}, font = 'title', style = Style.text('title', wide and Style.type.display or Style.type.title)},
        ui.label{part = 'caption', bind = {text = 'caption'}, color = 'textMuted'},
        ui.label{part = 'description', bind = {text = 'description'}, font = 'caption', color = 'textMuted'},
        ui.row{gap = space.s, margin = {space.s, 0, 0, 0},
            Widgets.playButton(nil, 64, {part = 'play'}),
            ui.button{part = 'shuffle', icon = Shell.icon('shuffle'), iconColor = 'text', variant = 'icon', accessibilityLabel = 'Shuffle', tooltip = 'Shuffle'},
        },
    }
    local head = wide and ui.row{gap = space.l, padding = {space.s, space.s, space.l, space.s}, alignItems = 'end', cover, texts} or ui.column{gap = space.m, padding = {space.xs, space.s, space.l, space.s}, cover, texts}
    self.gui:replaceChildren('body', {Shell.centered(1100, ui.collection{id = 'tracks', gap = 0, padding = {0, space.s, space.xl, space.s}, rememberFocus = true,
        types = {
            head = {template = head, interactive = false},
            track = Widgets.trackRow(source.type ~= 'album'),
        },
        onClick = function(event)
            if event.part == 'play' then
                Player.setShuffle(false)
                Player.playList(source.tracks, 1)
            elseif event.part == 'shuffle' then
                Player.setShuffle(true)
                Player.playList(source.tracks, Player.random:integer(1, math.max(1, #source.tracks)))
            end
        end,
        onSelect = function(event)
            self.onTracks(event)
        end,
        ui.emptyState{image = Shell.icon('music'), imageSize = 72, title = 'No songs yet', message = 'Add songs from their menu.'},
    }, 1)})
    local kinds = {album = 'Album', playlist = 'Playlist', genre = 'Genre'}
    local items = {{id = 'head', type = 'head', cover = source.cover, name = source.name, kind = kinds[source.type] or 'Playlist', caption = source.caption, description = source.description or ''}}
    for _, track in ipairs(source.tracks) do
        items[#items + 1] = track
    end
    self.gui:collection('tracks'):setItems(items)
end

-- The album, the playlist or the mix with the id, or the songs of a genre for an id that starts with "genre-".
function Collection:source(id)
    local genreId = id:match('^genre%-(.+)$')
    if not genreId then
        return Catalog.find(id)
    end
    local genre = Catalog.genre(genreId)
    local tracks = {}
    for _, track in ipairs(Catalog.tracks) do
        if track.genre == genreId then
            tracks[#tracks + 1] = track
        end
    end
    local mix = Catalog.mixes[1]
    for _, other in ipairs(Catalog.mixes) do
        if other.id == 'mix-' .. genreId then
            mix = other
        end
    end
    return {type = 'genre', genre = genreId, name = genre.name, caption = string.format('%s songs', Catalog.thousands(#tracks)), description = 'The ' .. string.lower(genre.name) .. ' of the whole library.', cover = mix.cover, tracks = tracks}
end

return Collection
