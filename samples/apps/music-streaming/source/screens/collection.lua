-- An album, a playlist, a mix or a genre: the cover, the names and the buttons that play and shuffle, over the songs in a recycled list, which holds all 2,400 liked songs.
local haylen = require('haylen')
local ui = require('haylen.ui')

local Catalog = require('data.catalog')
local Page = require('page')
local Player = require('player')
local Shell = require('shell')
local Widgets = require('widgets')

local Collection = haylen.class('Collection', Page)

function Collection:enter()
    local source = self:source(self.params.collection)
    self.source = source
    local items = {{id = 'head', type = 'head', cover = source.cover, name = source.name, caption = source.caption, description = source.description or ''}}
    for _, track in ipairs(source.tracks) do
        items[#items + 1] = track
    end
    self.onTracks = Widgets.onTracks(function() return source.tracks end, self)
    self:mountGui(ui.column{gap = 0, onCancel = Shell.back,
        Shell.header(source.name),
        Shell.centered(1000, ui.collection{id = 'tracks', gap = 2, padding = {0, 12, 24, 12}, rememberFocus = true,
            types = {
                head = {template = ui.column{gap = 10, padding = {4, 8, 14, 8},
                    ui.row{gap = 20, wrap = true,
                        ui.image{part = 'cover', bind = {image = 'cover'}, image = Shell.blankCover, width = 220, height = 220, radius = 18},
                        ui.column{gap = 4, minWidth = 260, grow = 1,
                            ui.label{part = 'name', bind = {text = 'name'}, font = 'title'},
                            ui.label{part = 'caption', bind = {text = 'caption'}, color = 'textMuted'},
                            ui.label{part = 'description', bind = {text = 'description'}, font = 'caption', color = 'textMuted'},
                            ui.row{gap = 10, margin = {10, 0, 0, 0},
                                ui.button{part = 'play', text = 'Play', icon = Shell.icon('play'), variant = 'primary'},
                                ui.button{part = 'shuffle', text = 'Shuffle', icon = Shell.icon('shuffle'), iconColor = 'text'},
                            },
                        },
                    },
                }, interactive = false},
                track = Widgets.trackRow(source.type ~= 'album'),
            },
            onClick = function(event)
                if event.part == 'play' then
                    Player.setShuffle(false)
                    Player.playList(source.tracks, 1)
                elseif event.part == 'shuffle' then
                    Player.shuffle = true
                    Player.playList(source.tracks, Player.random:integer(1, math.max(1, #source.tracks)))
                end
            end,
            onSelect = function(event)
                self.onTracks(event)
            end,
            ui.emptyState{image = Shell.icon('music'), imageSize = 72, title = 'No songs yet', message = 'Add songs from their menu.'},
        }, 1),
        Shell.playerBar(),
        Widgets.playlistDialog(self),
    })
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
    return {type = 'genre', name = genre.name, caption = string.format('%s songs', Catalog.thousands(#tracks)), description = 'The ' .. string.lower(genre.name) .. ' of the whole library.', cover = mix.cover, tracks = tracks}
end

return Collection
