-- The search tab: the genres to browse while the field is empty, and the artists, albums and songs that match the typed text in one recycled list.
local haylen = require('haylen')
local ui = require('haylen.ui')

local Catalog = require('data.catalog')
local Page = require('page')
local Shell = require('shell')
local Widgets = require('widgets')

local Search = haylen.class('Search', Page)

Search.limit = 400

function Search:enter()
    self.query = ''
    local tiles = {}
    for _, genre in ipairs(Catalog.genres) do
        tiles[#tiles + 1] = ui.button{id = 'genre-' .. genre.id, text = genre.name, height = 110, align = 'stretch', justify = 'start', style = {colors = {raised = genre.colors[1], border = genre.colors[1], text = '#FF1D1533', hover = '#22FFFFFF'}, metrics = {controlRadius = 18}, fonts = {button = {size = 26}}}, onClick = function()
            Shell.open(require('screens.collection'), {collection = 'genre-' .. genre.id})
        end}
    end
    self:mountGui(Shell.frame('search', ui.column{grow = 1, gap = 12, padding = {14, 0, 0, 0},
        ui.row{padding = {0, 20}, ui.filterField{id = 'query', placeholder = 'Artists, albums or songs', grow = 1, returnKey = 'search', onChange = function(event)
            self.query = string.lower(event.value)
            self:refresh()
        end}},
        ui.scroll{id = 'browse', grow = 1, height = 0,
            Shell.centered(1300, ui.column{gap = 12, padding = {0, 20, 24, 20},
                ui.sectionTitle{text = 'Browse all'},
                ui.grid{minColumnWidth = 220, columns = 6, gap = 14, children = tiles},
            }),
        },
        ui.collection{id = 'results', visible = false, grow = 1, gap = 2, padding = {0, 12, 24, 12}, rememberFocus = true,
            types = {
                artist = {template = ui.row{gap = 14, padding = {6, 8},
                    ui.image{part = 'cover', bind = {image = 'cover'}, image = Shell.blankCover, width = 56, height = 56, radius = 28},
                    ui.column{gap = 0, grow = 1, ui.label{part = 'name', bind = {text = 'name'}, font = 'button', wrap = false}, ui.label{text = 'Artist', font = 'caption', color = 'textMuted'}},
                }, estimatedSize = 68},
                album = {template = ui.row{gap = 14, padding = {6, 8},
                    ui.image{part = 'cover', bind = {image = 'cover'}, image = Shell.blankCover, width = 56, height = 56, radius = 8},
                    ui.column{gap = 0, grow = 1, ui.label{part = 'name', bind = {text = 'name'}, font = 'button', wrap = false}, ui.label{part = 'caption', bind = {text = 'caption'}, font = 'caption', color = 'textMuted', wrap = false}},
                }, estimatedSize = 68},
                track = Widgets.trackRow(true),
            },
            onSelect = function(event)
                local found = Catalog.find(event.part == 'menu' and event.cell.item or event.item)
                if found.type == 'artist' then
                    Shell.open(require('screens.artist'), {artist = found.id})
                elseif found.type == 'album' then
                    Shell.open(require('screens.collection'), {collection = found.id})
                else
                    self.onTracks(event)
                end
            end,
            ui.emptyState{image = Shell.icon('music'), imageSize = 72, title = 'Nothing found', message = 'Try another name.'},
        },
        Widgets.playlistDialog(self),
    }))
    self.results = self.gui:collection('results')
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
