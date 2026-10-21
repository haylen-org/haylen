-- An artist: the round picture, the listeners and the follow button under the glow of the genre, the most played songs and the albums, in one recycled grid of mixed cells.
local haylen = require('haylen')
local ui = require('haylen.ui')

local Catalog = require('data.catalog')
local Page = require('page')
local Player = require('player')
local Shell = require('shell')
local Style = require('style')
local Widgets = require('widgets')

local Artist = haylen.class('Artist', Page)

Artist.following = {}

function Artist:enter()
    local artist = Catalog.find(self.params.artist)
    self.artist = artist
    self.glow = artist.genre
    local popular = {}
    for index = 1, math.min(5, #artist.tracks) do
        popular[index] = artist.tracks[index]
    end
    self.popular = popular
    self.onTracks = Widgets.onTracks(function() return popular end, self)
    self:mountGui(Shell.frame(Shell.current, ui.column{grow = 1, gap = 0,
        Shell.header(''),
        ui.column{id = 'body', grow = 1, gap = 0},
        Widgets.playlistDialog(self),
    }))
end

function Artist:layoutChanged(wide)
    local space = Style.space
    local artist = self.artist
    local picture = ui.image{part = 'cover', bind = {image = 'cover'}, image = Shell.blankCover, width = wide and 220 or 200, height = wide and 220 or 200, radius = 120, align = wide and 'start' or 'center'}
    local texts = ui.column{gap = space.xxs, grow = 1, minWidth = 260,
        ui.label{text = 'Artist', font = 'caption', color = 'textMuted'},
        ui.label{part = 'name', bind = {text = 'name'}, font = 'title', style = Style.text('title', wide and Style.type.display or Style.type.title)},
        ui.label{part = 'caption', bind = {text = 'caption'}, color = 'textMuted'},
        ui.row{gap = space.s, margin = {space.s, 0, 0, 0},
            Widgets.playButton(nil, 64, {part = 'play'}),
            ui.button{part = 'follow', bind = {text = 'followText'}, text = 'Follow', style = {colors = {raised = '#00000000', border = Style.color.muted}}},
        },
    }
    local head = wide and ui.row{gap = space.l, padding = {space.s, space.s, space.l, space.s}, alignItems = 'end', picture, texts} or ui.column{gap = space.m, padding = {space.xs, space.s, space.l, space.s}, picture, texts}
    self.gui:replaceChildren('body', {Shell.centered(1100, ui.collection{id = 'items', grow = 1, layout = 'grid', minCellSize = 200, gap = space.xs, padding = {0, space.s, space.xl, space.s}, rememberFocus = true,
        types = {
            head = {template = head, span = 'full', interactive = false},
            section = {template = ui.label{part = 'title', bind = {text = 'title'}, font = 'heading', margin = {space.m, space.s, space.xs, space.s}}, span = 'full', interactive = false},
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
                Widgets.openItem(found.id)
            else
                self.onTracks(event)
            end
        end,
    }, 1)})
    local items = {{id = 'head', type = 'head', cover = artist.cover, name = artist.name, caption = artist.caption}, {id = 'popular', type = 'section', title = 'Popular'}}
    for _, track in ipairs(self.popular) do
        items[#items + 1] = track
    end
    items[#items + 1] = {id = 'albums', type = 'section', title = 'Albums'}
    for _, album in ipairs(artist.albums) do
        items[#items + 1] = album
    end
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
