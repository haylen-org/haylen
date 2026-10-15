-- The home tab: a greeting with the settings, the playlists, the daily mixes, the new releases, the popular artists and the albums to jump back into, in horizontal shelves.
local haylen = require('haylen')
local preferences = require('haylen.preferences')
local ui = require('haylen.ui')

local Catalog = require('data.catalog')
local Page = require('page')
local Shell = require('shell')
local Widgets = require('widgets')

local Home = haylen.class('Home', Page)

Home.cardWidth = 200

function Home:enter()
    self:mountGui(Shell.frame('home', ui.column{grow = 1, gap = 0,
        ui.row{gap = 12, padding = {14, 16, 6, 20},
            ui.label{text = 'Good evening', font = 'title', grow = 1, wrap = false},
            ui.button{id = 'settings', icon = Shell.icon('settings'), variant = 'icon', tooltip = 'Settings', accessibilityLabel = 'Settings', onClick = function(event)
                event.gui:set('settingsDialog', {open = true})
            end},
        },
        ui.scroll{grow = 1, height = 0,
            ui.column{maxWidth = 1300, align = 'center', gap = 14, padding = {4, 20, 28, 20},
                self:banner(),
                ui.sectionTitle{text = 'Your playlists'},
                self:shelf('playlists', Widgets.coverCard(Home.cardWidth), 'playlist'),
                ui.sectionTitle{text = 'Made for you'},
                self:shelf('mixes', Widgets.coverCard(Home.cardWidth), 'playlist'),
                ui.sectionTitle{text = 'New releases'},
                self:shelf('releases', Widgets.coverCard(Home.cardWidth), 'album'),
                ui.sectionTitle{text = 'Popular artists'},
                self:shelf('artists', Widgets.artistCard(170), 'artist'),
                ui.sectionTitle{text = 'Jump back in'},
                self:shelf('albums', Widgets.coverCard(Home.cardWidth), 'album'),
            },
        },
        ui.dialog{id = 'settingsDialog', title = 'Settings', buttons = {{id = 'close', text = 'Close'}},
            ui.toggle{id = 'day', text = 'Light theme', checked = ui.theme() == 'musicDay', onChange = function(event)
                preferences.set('day', event.checked)
                ui.setTheme(event.checked and 'musicDay' or 'musicNight')
            end},
            ui.toggle{id = 'crossfade', text = 'Crossfade between songs', checked = true},
        },
    }))
    local releases, albums = {}, {}
    for index = #Catalog.albums, 1, -1 do
        local album = Catalog.albums[index]
        if album.year >= 2025 and #releases < 12 then
            releases[#releases + 1] = album
        end
    end
    for index = 1, 12 do
        albums[index] = Catalog.albums[index * 7]
    end
    local artists = {}
    for index = 1, 12 do
        artists[index] = Catalog.artists[index * 3]
    end
    self.gui:collection('playlists'):setItems(Catalog.playlists)
    self.gui:collection('mixes'):setItems(Catalog.mixes)
    self.gui:collection('releases'):setItems(releases)
    self.gui:collection('artists'):setItems(artists)
    self.gui:collection('albums'):setItems(albums)
end

-- The card of the week with the illustration of the app.
function Home:banner()
    return ui.card{padding = {18, 12, 18, 24}, style = {colors = {raised = 'accent', border = 'accent'}},
        ui.row{gap = 8,
            ui.column{gap = 6, grow = 1,
                ui.label{text = 'Fresh sounds every morning', font = 'heading', color = 'onAccent'},
                ui.label{text = 'Every song here is made by the app itself, so no two libraries sound alike.', font = 'caption', color = 'onAccent'},
                ui.button{id = 'playMix', text = 'Play your mix', icon = Shell.icon('play'), align = 'start', margin = {6, 0, 0, 0}, style = {colors = {raised = '#FFFFFFFF', border = '#FFFFFFFF', text = '#FF1D1533'}}, iconColor = 'accent', onClick = function()
                    require('player').playList(Catalog.mixes[1].tracks, 1)
                end},
            },
            ui.image{image = 'images/hero.png', width = 220, height = 165, fit = 'contain', accessibilityHidden = true},
        },
    }
end

function Home:shelf(id, card, kind)
    return ui.collection{id = id, axis = 'horizontal', scrollbar = false, gap = 16, rememberFocus = true, types = {[kind] = card}, onSelect = function(event)
        if kind == 'artist' then
            Shell.open(require('screens.artist'), {artist = event.item})
        else
            Shell.open(require('screens.collection'), {collection = event.item})
        end
    end}
end

return Home
