-- The library tab: the playlists, the albums and the artists of the person in one recycled grid, and a dialog that makes a new playlist.
local haylen = require('haylen')
local ui = require('haylen.ui')

local Catalog = require('data.catalog')
local Page = require('page')
local Shell = require('shell')
local Widgets = require('widgets')

local Library = haylen.class('Library', Page)

Library.sections = {{id = 'playlists', text = 'Playlists'}, {id = 'albums', text = 'Albums'}, {id = 'artists', text = 'Artists'}}

function Library:enter()
    self.section = 'playlists'
    self.created = 0
    self:mountGui(Shell.frame('library', ui.column{grow = 1, gap = 10, padding = {14, 0, 0, 0},
        ui.row{gap = 12, padding = {0, 16, 0, 20},
            ui.label{text = 'Your library', font = 'title', grow = 1, wrap = false},
            ui.button{id = 'new', icon = Shell.icon('plus'), variant = 'icon', tooltip = 'New playlist', accessibilityLabel = 'New playlist', onClick = function(event)
                event.gui:set('newDialog', {open = true})
            end},
        },
        ui.row{padding = {0, 20}, ui.segmentedControl{id = 'section', items = Library.sections, selected = 'playlists', grow = 1, maxWidth = 640, onChange = function(event)
            self.section = event.value
            self:show()
        end}},
        ui.collection{id = 'items', grow = 1, layout = 'grid', minCellSize = 190, gap = 16, padding = {4, 20, 24, 20}, rememberFocus = true,
            types = {playlist = Widgets.coverCard(), album = Widgets.coverCard(), artist = {template = ui.column{gap = 6, padding = {0, 0, 8, 0},
                ui.image{part = 'cover', bind = {image = 'cover'}, image = Shell.blankCover, align = 'stretch', aspectRatio = 1, radius = 400},
                ui.label{part = 'name', bind = {text = 'name'}, font = 'button', textAlign = 'center', align = 'stretch', wrap = false},
            }}},
            onSelect = function(event)
                local found = Catalog.find(event.item)
                if found.type == 'artist' then
                    Shell.open(require('screens.artist'), {artist = found.id})
                else
                    Shell.open(require('screens.collection'), {collection = found.id})
                end
            end,
        },
        ui.dialog{id = 'newDialog', title = 'New playlist', buttons = {{id = 'cancel', text = 'Cancel'}, {id = 'create', text = 'Create', variant = 'primary'}},
            onAnswer = function(event)
                if event.button == 'create' then
                    self:create()
                end
            end,
            ui.textField{id = 'newName', placeholder = 'My playlist', maxLength = 40, onChange = function(event)
                self.newName = event.value
            end},
        },
    }))
    self.items = self.gui:collection('items')
    self:show()
end

function Library:show()
    local lists = {playlists = Catalog.playlists, albums = {}, artists = {}}
    for index = 1, 60 do
        lists.albums[index] = Catalog.albums[index * 5]
        lists.artists[index] = Catalog.artists[(index * 11) % #Catalog.artists + 1]
    end
    local seen, artists = {}, {}
    for _, artist in ipairs(lists.artists) do
        if not seen[artist] then
            seen[artist] = true
            artists[#artists + 1] = artist
        end
    end
    lists.artists = artists
    self.items:setItems(lists[self.section])
end

function Library:create()
    self.created = self.created + 1
    local name = (self.newName or ''):match('^%s*(.-)%s*$')
    local playlist = {id = 'created-' .. self.created, type = 'playlist', name = name ~= '' and name or 'My playlist ' .. self.created, description = 'A playlist you made.', tracks = {}, cover = Shell.blankCover, caption = 'Playlist · 0 songs'}
    table.insert(Catalog.playlists, 2, playlist)
    Catalog.byId[playlist.id] = playlist
    Shell.notify(playlist.name .. ' is in your library.')
    self:show()
end

return Library
