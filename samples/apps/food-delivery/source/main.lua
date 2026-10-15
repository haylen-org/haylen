-- Food Delivery: restaurants of a made-up city, their menus, a cart, the checkout and the order on a map of the city, in a warm tomato red look with a light and a dark theme.
local assets = require('haylen.assets')
local events = require('haylen.events')
local graphics = require('haylen.graphics')
local haylen = require('haylen')
local preferences = require('haylen.preferences')
local system = require('haylen.system')
local ui = require('haylen.ui')

local Shell = require('shell')
local Tour = require('tour')

ui.addFont('poppins', graphics.newFontFamily({regular = assets.font('fonts/poppins-regular.ttf'), bold = assets.font('fonts/poppins-semibold.ttf')}))

local fonts = {
    body = {font = 'poppins', size = 23},
    caption = {font = 'poppins', size = 19},
    button = {font = 'poppins', size = 22, bold = true},
    heading = {font = 'poppins', size = 29, bold = true},
    title = {font = 'poppins', size = 40, bold = true},
}

local metrics = {
    controlHeight = 60,
    controlRadius = 18,
    controlPaddingX = 22,
    panelRadius = 24,
    panelPadding = 20,
    cellRadius = 22,
    itemSpacing = 12,
    iconSize = 30,
    chipHeight = 46,
    listRowHeight = 64,
    dialogWidth = 660,
    toastWidth = 560,
    shadowSize = 22,
    shadowOffset = 5,
    badgePaddingX = 12,
}

ui.addTheme({
    name = 'foodLight',
    imageFilter = 'linear',
    fonts = fonts,
    metrics = metrics,
    colors = {
        window = '#FFFFF8F1',
        panel = '#FFFFF8F1',
        raised = '#FFFFFFFF',
        border = '#FFF2E3D5',
        borderStrong = '#FFE3CDB9',
        text = '#FF2A1E1A',
        textMuted = '#FF8A766B',
        accent = '#FFE8452C',
        accentHover = '#FFF0583F',
        accentStrong = '#FFC93620',
        accentBackground = '#24E8452C',
        accentText = '#FFD03A22',
        selection = '#2EE8452C',
        focus = '#FF2F86F6',
        track = '#FFEEDDCE',
        scrollbar = '#FFE3CDB9',
        scrollbarHover = '#FFD3B9A2',
        success = '#FF2E9E62',
        successText = '#FF1F7A49',
        warning = '#FFF2A93B',
        information = '#FF2F86F6',
        shadow = '#24402010',
    },
}, 'light')

ui.addTheme({
    name = 'foodDark',
    imageFilter = 'linear',
    fonts = fonts,
    metrics = metrics,
    colors = {
        window = '#FF1C1512',
        panel = '#FF1C1512',
        raised = '#FF2A201B',
        border = '#FF3A2D26',
        borderStrong = '#FF52423A',
        text = '#FFF7EDE6',
        textMuted = '#FFB59E91',
        accent = '#FFF0553B',
        accentHover = '#FFF66C53',
        accentStrong = '#FFD8442B',
        accentBackground = '#33F0553B',
        accentText = '#FFFF8A70',
        selection = '#3DF0553B',
        focus = '#FF7AB0FF',
        track = '#FF3A2D26',
        scrollbar = '#FF4A3A31',
        scrollbarHover = '#FF5E4B40',
        success = '#FF3DBE7A',
        warning = '#FFF2B23A',
    },
}, 'dark')

-- The interface keeps its size in points on every screen, and a television, seen from the couch, draws it half as large again.
if system.info().deviceKind == 'tv' then
    ui.setScale(1.5)
end
ui.setTheme(preferences.get('dark', system.theme() == 'dark') and 'foodDark' or 'foodLight')

events.on('notice', function(text)
    Shell.notify(text, 'information')
end)

Shell.start()

if haylen.platform == 'headless' then
    Tour.start()
end
