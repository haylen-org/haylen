-- An artist: the picture, the listeners and the follow button, the most played songs and the albums, in one recycled grid of mixed cells.
local haylen = require('haylen')
local ui = require('haylen.ui')

local Catalog = require('data.catalog')
local Page = require('page')
local Player = require('player')
local Shell = require('shell')
local Widgets = require('widgets')

local Artist = haylen.class('Artist', Page)

Artist.following = {}

function Artist:enter()
    local artist = Catalog.find(self.params.artist)
    self.artist = artist
    local popular = {}
    for index = 1, math.min(5, #artist.tracks) do
        popular[index] = artist.tracks[index]
    end
    self.popular = popular
    local items = {{id = 'head', type = 'head', cover = artist.cover, name = artist.name, caption = artist.caption}, {id = 'popular', type = 'section', title = 'Popular'}}
    for _, track in ipairs(popular) do
        items[#items + 1] = track
    end
    items[#items + 1] = {id = 'albums', type = 'section', title = 'Albums'}
    for _, album in ipairs(artist.albums) do
        items[#items + 1] = album
    end
    self.onTracks = Widgets.onTracks(function() return popular end, self)
    self:mountGui(ui.column{gap = 0, onCancel = Shell.back,
        Shell.header(artist.name),
        Shell.centered(1100, ui.collection{id = 'items', layout = 'grid', minCellSize = 200, gap = 14, padding = {0, 16, 24, 16}, rememberFocus = true,
            types = {
                head = {template = ui.row{gap = 20, padding = {4, 4, 10, 4}, wrap = true,
                    ui.image{part = 'cover', bind = {image = 'cover'}, image = Shell.blankCover, width = 200, height = 200, radius = 100},
                    ui.column{gap = 6, minWidth = 260, grow = 1,
                        ui.label{part = 'name', bind = {text = 'name'}, font = 'title'},
                        ui.label{part = 'caption', bind = {text = 'caption'}, color = 'textMuted'},
                        ui.row{wrap = true, gap = 10, margin = {8, 0, 0, 0},
                            ui.button{part = 'play', text = 'Play', icon = Shell.icon('play'), variant = 'primary'},
                            ui.button{part = 'follow', bind = {text = 'followText'}, text = 'Follow'},
                        },
                    },
                }, span = 'full', interactive = false},
                section = {template = ui.sectionTitle{part = 'title', bind = {text = 'title'}}, span = 'full', interactive = false},
                track = Widgets.trackRow(true, 'full'),
                album = Widgets.coverCard(),
            },
            onClick = function(event)
                if event.part == 'play' then
                    Player.playList(artist.tracks, 1)
                elseif event.part == 'follow' then
                    Artist.following[artist.id] = not Artist.following[artist.id] or nil
                    self:showFollow()
                end
            end,
            onSelect = function(event)
                local found = Catalog.find(event.part == 'menu' and event.cell.item or event.item)
                if found.type == 'album' then
                    Shell.open(require('screens.collection'), {collection = found.id})
                else
                    self.onTracks(event)
                end
            end,
        }, 1),
        Shell.playerBar(),
        Widgets.playlistDialog(self),
    })
    self.items = items
    self.list = self.gui:collection('items')
    self.list:setItems(items)
    self:showFollow()
end

function Artist:showFollow()
    self.items[1].followText = Artist.following[self.artist.id] and 'Following' or 'Follow'
    self.list:reload('head')
end

return Artist
