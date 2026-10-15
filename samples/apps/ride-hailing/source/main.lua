-- Ride Hailing: rides across a made-up city at night, with live traffic on the map, the search of a destination, six kinds of ride, the whole trip from the search for a driver to the rating, the activity, the wallet and the account, in a night look with lime highlights and a day theme.
local assets = require('haylen.assets')
local events = require('haylen.events')
local graphics = require('haylen.graphics')
local haylen = require('haylen')
local preferences = require('haylen.preferences')
local system = require('haylen.system')
local ui = require('haylen.ui')

local Shell = require('shell')
local Tour = require('tour')

ui.addFont('barlow', graphics.newFontFamily({regular = assets.font('fonts/barlow-regular.ttf'), bold = assets.font('fonts/barlow-semibold.ttf')}))

local fonts = {
    body = {font = 'barlow', size = 24},
    caption = {font = 'barlow', size = 20},
    button = {font = 'barlow', size = 23, bold = true},
    heading = {font = 'barlow', size = 30, bold = true},
    title = {font = 'barlow', size = 40, bold = true},
}

local metrics = {
    controlHeight = 60,
    controlRadius = 16,
    controlPaddingX = 20,
    panelRadius = 26,
    panelPadding = 20,
    cellRadius = 18,
    itemSpacing = 12,
    iconSize = 28,
    chipHeight = 44,
    listRowHeight = 64,
    dialogWidth = 640,
    toastWidth = 560,
    shadowSize = 26,
    shadowOffset = 6,
}

ui.addTheme({
    name = 'rideNight',
    imageFilter = 'linear',
    fonts = fonts,
    metrics = metrics,
    colors = {
        window = '#FF0B1020',
        panel = '#FF0E1426',
        raised = '#FF161D33',
        border = '#FF243052',
        borderStrong = '#FF34406A',
        text = '#FFEAF0FF',
        textMuted = '#FF8C97B8',
        accent = '#FFB6F24A',
        accentHover = '#FFC6F76A',
        accentStrong = '#FF9ED636',
        onAccent = '#FF0B1020',
        accentBackground = '#2EB6F24A',
        accentText = '#FFC6F76A',
        selection = '#33B6F24A',
        focus = '#FFFFFFFF',
        track = '#FF243052',
        scrollbar = '#FF2C3860',
        scrollbarHover = '#FF3C4A78',
        tooltip = '#F2EAF0FF',
        onTooltip = '#FF0B1020',
        shadow = '#99000000',
    },
}, 'dark')

ui.addTheme({
    name = 'rideDay',
    imageFilter = 'linear',
    fonts = fonts,
    metrics = metrics,
    colors = {
        window = '#FFF2F5FA',
        panel = '#FFF2F5FA',
        raised = '#FFFFFFFF',
        border = '#FFDDE3EE',
        borderStrong = '#FFC5CEDF',
        text = '#FF111830',
        textMuted = '#FF5E6886',
        accent = '#FF5BAA1A',
        accentHover = '#FF6ABD24',
        accentStrong = '#FF4A9012',
        accentBackground = '#245BAA1A',
        accentText = '#FF3F7F10',
        selection = '#2E5BAA1A',
        focus = '#FF2F86F6',
        track = '#FFDDE3EE',
        scrollbar = '#FFC5CEDF',
        scrollbarHover = '#FFAEB9CF',
        shadow = '#2A111830',
    },
}, 'light')

-- The interface keeps its size in points on every screen, and a television, seen from the couch, draws it half as large again.
if system.info().deviceKind == 'tv' then
    ui.setScale(1.5)
end
ui.setTheme(preferences.get('day', false) and 'rideDay' or 'rideNight')

events.on('notice', function(text)
    Shell.notify(text, 'information')
end)

Shell.start()

if haylen.platform == 'headless' then
    Tour.start()
end
