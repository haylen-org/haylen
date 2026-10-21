-- The search tab: the genres to browse as tiles in their colors while the field is empty, and the artists, albums and songs that match the typed text in one recycled list.
local haylen = require('haylen')
local ui = require('haylen.ui')

local Catalog = require('data.catalog')
local Covers = require('covers')
local Page = require('page')
local Shell = require('shell')
local Style = require('style')
local Widgets = require('widgets')

local Search = haylen.class('Search', Page)

Search.limit = 400

function Search:enter()
    local space = Style.space
    self.query = ''
    local tiles = {}
    for _, genre in ipairs(Catalog.genres) do
        local shade = Covers.deepText(genre.id, 0.8)
        tiles[#tiles + 1] = ui.button{id = 'genre-' .. genre.id, text = genre.name, height = 132, align = 'stretch', justify = 'start',
            style = {colors = {raised = shade, border = shade, text = Style.color.text, hover = '#1FFFFFFF'}, metrics = {controlRadius = Style.radius.card, controlPaddingX = space.l}, fonts = {button = {size = Style.type.heading}}},
            onClick = function()
                Shell.open(require('screens.collection'), {collection = 'genre-' .. genre.id})
            end}
    end
    self:mountGui(Shell.frame('search', ui.column{grow = 1, gap = space.s, padding = {space.l, 0, 0, 0},
        Shell.centered(Style.size.page, ui.column{gap = space.s, padding = {0, space.l},
            ui.label{text = 'Search', font = 'title'},
            ui.filterField{id = 'query', placeholder = 'Artists, albums or songs', returnKey = 'search', onChange = function(event)
                self.query = string.lower(event.value)
                self:refresh()
            end},
        }),
        ui.scroll{id = 'browse', grow = 1, height = 0,
            Shell.centered(Style.size.page, ui.column{gap = space.s, padding = {space.s, space.l, space.xl, space.l},
                ui.label{text = 'Browse all', font = 'heading'},
                ui.grid{minColumnWidth = 200, columns = 6, gap = space.s, children = tiles},
            }),
        },
        ui.row{id = 'results', visible = false, grow = 1, gap = 0, align = 'stretch', Shell.centered(Style.size.page, ui.collection{id = 'resultList', grow = 1, gap = 0, padding = {0, space.s, space.xl, space.s}, rememberFocus = true,
            types = {
                artist = {template = ui.row{gap = space.m, padding = {space.xs, space.s},
                    ui.image{part = 'cover', bind = {image = 'cover'}, image = Shell.blankCover, width = 60, height = 60, radius = 30},
                    ui.column{gap = 0, grow = 1, ui.label{part = 'name', bind = {text = 'name'}, wrap = false}, ui.label{text = 'Artist', font = 'caption', color = 'textMuted'}},
                }, estimatedSize = 84},
                album = {template = ui.row{gap = space.m, padding = {space.xs, space.s},
                    ui.image{part = 'cover', bind = {image = 'cover'}, image = Shell.blankCover, width = 60, height = 60, radius = Style.radius.cover},
                    ui.column{gap = 0, grow = 1, ui.label{part = 'name', bind = {text = 'name'}, wrap = false}, ui.label{part = 'caption', bind = {text = 'caption'}, font = 'caption', color = 'textMuted', wrap = false}},
                }, estimatedSize = 84},
                track = Widgets.trackRow(true),
            },
            onSelect = function(event)
                local found = Catalog.find(event.part == 'menu' and event.cell.item or event.item)
                if found.type == 'artist' or found.type == 'album' then
                    Widgets.openItem(found.id)
                else
                    self.onTracks(event)
                end
            end,
            ui.emptyState{image = Shell.icon('music'), imageSize = 72, title = 'Nothing found', message = 'Try another name.'},
        }, 1)},
        Widgets.playlistDialog(self),
    }))
    self.results = self.gui:collection('resultList')
    self.onTracks = Widgets.onTracks(function() return self.tracks end, self)
end

function Search:refresh()
    local searching = self.query ~= ''
    self.gui:set('browse', {visible = not searching})
    self.gui:set('results', {visible = searching})
    if not searching then
        return
    end
    local items, tracks = {}, {}
    for _, artist in ipairs(Catalog.artists) do
        if string.find(string.lower(artist.name), self.query, 1, true) then
            items[#items + 1] = artist
        end
    end
    for _, album in ipairs(Catalog.albums) do
        if string.find(string.lower(album.name .. ' ' .. album.artistName), self.query, 1, true) then
            items[#items + 1] = album
        end
    end
    for _, track in ipairs(Catalog.tracks) do
        if #tracks < Search.limit and string.find(track.searchText, self.query, 1, true) then
            tracks[#tracks + 1] = track
            items[#items + 1] = track
        end
    end
    self.tracks = tracks
    self.results:setItems(items)
end

return Search
