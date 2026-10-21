-- The library tab: the playlists, the albums and the artists of the person in one recycled grid, picked with pills, a dialog that makes a new playlist and, where quitting closes the app, a button that quits it.
local haylen = require('haylen')
local ui = require('haylen.ui')

local Catalog = require('data.catalog')
local Page = require('page')
local Shell = require('shell')
local Style = require('style')
local Widgets = require('widgets')

local Library = haylen.class('Library', Page)

Library.sections = {{id = 'playlists', text = 'Playlists'}, {id = 'albums', text = 'Albums'}, {id = 'artists', text = 'Artists'}}

function Library:enter()
    local space = Style.space
    self.section = 'playlists'
    self.created = 0
    local pills = {gap = space.xs}
    for _, section in ipairs(Library.sections) do
        pills[#pills + 1] = ui.chip{id = 'section-' .. section.id, text = section.text, selected = section.id == 'playlists', onChange = function()
            self:pick(section.id)
        end}
    end
    self:mountGui(Shell.frame('library', ui.column{grow = 1, gap = space.s, padding = {space.l, 0, 0, 0},
        Shell.centered(Style.size.page, ui.column{gap = space.s, padding = {0, space.l},
            ui.row{gap = space.xs,
                ui.label{text = 'Library', font = 'title', grow = 1, wrap = false},
                ui.button{id = 'new', icon = Shell.icon('plus'), iconColor = 'text', variant = 'icon', tooltip = 'New playlist', accessibilityLabel = 'New playlist', onClick = function(event)
                    event.gui:set('newDialog', {open = true})
                end},
                -- Where quitting closes the app, the library offers it, so the way the app ends can be tried.
                haylen.canQuit() and ui.button{id = 'quit', text = 'Quit', variant = 'toolbar', onClick = function() haylen.quit() end} or nil,
            },
            ui.row(pills),
        }),
        Shell.centered(Style.size.page, ui.collection{id = 'items', grow = 1, layout = 'grid', minCellSize = 190, gap = space.xs, padding = {space.xs, space.m, space.xl, space.m}, rememberFocus = true,
            types = {playlist = Widgets.coverCard(), album = Widgets.coverCard(), artist = Widgets.artistCard()},
            onSelect = function(event)
                Widgets.openItem(event.item)
            end,
        }, 1),
        ui.dialog{id = 'newDialog', title = 'New playlist', buttons = {{id = 'cancel', text = 'Cancel'}, {id = 'create', text = 'Create', variant = 'primary'}},
            onAnswer = function(event)
                if event.button == 'create' then
                    self:create()
                end
            end,
            ui.textField{id = 'newName', placeholder = 'My playlist', maxLength = 40, autocapitalize = 'words', returnKey = 'done', onChange = function(event)
                self.newName = event.value
            end},
        },
    }))
    self.items = self.gui:collection('items')
    self:show()
end

function Library:pick(id)
    self.gui:set('section-' .. self.section, {selected = false})
    self.section = id
    self.gui:set('section-' .. id, {selected = true})
    self:show()
end

function Library:show()
    local lists = {playlists = Catalog.playlists, albums = {}, artists = {}}
    local seen = {}
    for index = 1, 60 do
        lists.albums[index] = Catalog.albums[index * 5]
        local artist = Catalog.artists[(index * 11) % #Catalog.artists + 1]
        if not seen[artist] then
            seen[artist] = true
            lists.artists[#lists.artists + 1] = artist
        end
    end
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
