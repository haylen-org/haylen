-- The home tab: a greeting, the mix of the day on a card lit by its colors, and shelves of the mixes, the playlists, new releases, popular artists and albums played lately.
local haylen = require('haylen')
local ui = require('haylen.ui')

local Catalog = require('data.catalog')
local Covers = require('covers')
local Page = require('page')
local Player = require('player')
local Shell = require('shell')
local Style = require('style')
local Widgets = require('widgets')

local Home = haylen.class('Home', Page)

function Home:enter()
    local space = Style.space
    self.glow = Catalog.mixes[1].genre
    self:mountGui(Shell.frame('home', ui.scroll{grow = 1, height = 0,
        Shell.centered(Style.size.page, ui.column{id = 'page', gap = space.s, padding = {space.l, space.m, space.xl, space.m},
            ui.label{text = Home.greeting(), font = 'title', margin = {0, space.xs, space.xs, space.xs}},
            ui.column{id = 'featured', margin = {0, space.xs}},
            Widgets.section('Your mixes'),
            Widgets.shelf('mixes', 'playlist'),
            Widgets.section('Your playlists'),
            Widgets.shelf('playlists', 'playlist'),
            Widgets.section('New releases'),
            Widgets.shelf('releases', 'album'),
            Widgets.section('Popular artists'),
            Widgets.shelf('artists', 'artist'),
            Widgets.section('Played lately'),
            Widgets.shelf('albums', 'album'),
        }),
    }))
    local releases, albums, artists = {}, {}, {}
    for index = #Catalog.albums, 1, -1 do
        local album = Catalog.albums[index]
        if album.year >= 2025 and #releases < 12 then
            releases[#releases + 1] = album
        end
    end
    for index = 1, 12 do
        albums[index] = Catalog.albums[index * 7]
        artists[index] = Catalog.artists[index * 3]
    end
    self.gui:collection('mixes'):setItems(Catalog.mixes)
    self.gui:collection('playlists'):setItems(Catalog.playlists)
    self.gui:collection('releases'):setItems(releases)
    self.gui:collection('artists'):setItems(artists)
    self.gui:collection('albums'):setItems(albums)
end

function Home.greeting()
    local hour = tonumber(os.date('%H'))
    return hour < 12 and 'Good morning' or (hour < 18 and 'Good afternoon' or 'Good evening')
end

-- The mix of the day: its cover beside its name and the button that plays it, over the colors of its genre, with the illustration of the app on wide windows.
function Home:layoutChanged(wide)
    local space = Style.space
    local mix = Catalog.mixes[1]
    local shade = Covers.deepText(mix.genre, 0.7)
    local card = ui.card{padding = wide and space.l or space.m, gap = space.m, style = {colors = {raised = shade, border = shade}, metrics = {panelRadius = Style.radius.card}},
        ui.row{gap = space.l,
            ui.image{image = mix.cover, width = wide and 180 or 120, height = wide and 180 or 120, radius = Style.radius.cover},
            ui.column{gap = space.xxs, grow = 1,
                ui.label{text = 'Mix of the day', font = 'caption', color = 'text'},
                ui.label{text = mix.name, font = 'title', wrap = false, style = Style.text('title', wide and Style.type.title or Style.type.heading)},
                ui.label{text = mix.description, color = 'text', visible = wide},
                Widgets.playButton('featuredPlay', wide and 64 or 56, {margin = {space.xs, 0, 0, 0}, onClick = function()
                    Player.playList(mix.tracks, 1)
                end}, true),
            },
            wide and ui.image{image = 'images/hero.png', width = 280, height = 210, fit = 'contain', accessibilityHidden = true} or nil,
        },
    }
    self.gui:replaceChildren('featured', {card})
    self.gui:set('page', {padding = wide and {space.xl, space.xl, space.xxl, space.xl} or {space.l, space.m, space.xl, space.m}})
end

return Home
