-- Music Streaming: a made-up library of songs that the app makes with its own synthesizer, with home shelves, search, genres, a library of playlists, albums and artists, a player bar, the now playing screen with seeking, shuffle, repeat and a sleep timer, and a queue to reorder, in a deep violet look with a light theme.
local assets = require('haylen.assets')
local events = require('haylen.events')
local graphics = require('haylen.graphics')
local haylen = require('haylen')
local preferences = require('haylen.preferences')
local system = require('haylen.system')
local ui = require('haylen.ui')

local Covers = require('covers')
local Shell = require('shell')
local Tour = require('tour')

ui.addFont('kanit', graphics.newFontFamily({regular = assets.font('fonts/kanit-regular.ttf'), bold = assets.font('fonts/kanit-semibold.ttf')}))

local fonts = {
    body = {font = 'kanit', size = 23},
    caption = {font = 'kanit', size = 19},
    button = {font = 'kanit', size = 22, bold = true},
    heading = {font = 'kanit', size = 29, bold = true},
    title = {font = 'kanit', size = 42, bold = true},
}

local metrics = {
    controlHeight = 58,
    controlRadius = 29,
    controlPaddingX = 22,
    panelRadius = 20,
    panelPadding = 20,
    cellRadius = 14,
    itemSpacing = 12,
    iconSize = 28,
    chipHeight = 44,
    listRowHeight = 64,
    dialogWidth = 620,
    toastWidth = 560,
    shadowSize = 24,
    shadowOffset = 6,
}

ui.addTheme({
    name = 'musicNight',
    imageFilter = 'linear',
    fonts = fonts,
    metrics = metrics,
    colors = {
        window = '#FF120E1F',
        panel = '#FF120E1F',
        raised = '#FF1E1832',
        border = '#FF2E2547',
        borderStrong = '#FF433863',
        text = '#FFF3EEFF',
        textMuted = '#FFA49BC0',
        accent = '#FF9B6BFF',
        accentHover = '#FFAD86FF',
        accentStrong = '#FF8455F0',
        accentBackground = '#339B6BFF',
        accentText = '#FFC3A6FF',
        selection = '#3D9B6BFF',
        focus = '#FFFFC93C',
        track = '#FF2E2547',
        scrollbar = '#FF362C55',
        scrollbarHover = '#FF4A3E70',
        shadow = '#99000000',
    },
}, 'dark')

ui.addTheme({
    name = 'musicDay',
    imageFilter = 'linear',
    fonts = fonts,
    metrics = metrics,
    colors = {
        window = '#FFF7F4FF',
        panel = '#FFF7F4FF',
        raised = '#FFFFFFFF',
        border = '#FFE6DEF7',
        borderStrong = '#FFD3C6F0',
        text = '#FF1D1533',
        textMuted = '#FF6E6488',
        accent = '#FF7A4BE8',
        accentHover = '#FF8B60F0',
        accentStrong = '#FF6A3BD6',
        accentBackground = '#247A4BE8',
        accentText = '#FF6436CF',
        selection = '#2E7A4BE8',
        focus = '#FF2F86F6',
        track = '#FFE6DEF7',
        scrollbar = '#FFD3C6F0',
        scrollbarHover = '#FFBFAEE8',
        shadow = '#241D1533',
    },
}, 'light')

-- The interface keeps its size in points on every screen, and a television, seen from the couch, draws it half as large again.
if system.info().deviceKind == 'tv' then
    ui.setScale(1.5)
end
ui.setTheme(preferences.get('day', false) and 'musicDay' or 'musicNight')
Covers.register()

events.on('notice', function(text)
    Shell.notify(text, 'information')
end)

Shell.start()

if haylen.platform == 'headless' then
    Tour.start()
end
