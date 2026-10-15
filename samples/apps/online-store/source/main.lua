-- Online Store: a made-up shop of clothes, shoes, bags and things for the home, with three thousand products in categories, product pages with galleries, colors and sizes, a bag with promo codes, the checkout, orders followed on a map of the city and a wishlist, in a coral and navy look with a light and a dark theme.
local assets = require('haylen.assets')
local events = require('haylen.events')
local graphics = require('haylen.graphics')
local haylen = require('haylen')
local preferences = require('haylen.preferences')
local system = require('haylen.system')
local ui = require('haylen.ui')

local Shell = require('shell')
local Tour = require('tour')

ui.addFont('hind', graphics.newFontFamily({regular = assets.font('fonts/hind-regular.ttf'), bold = assets.font('fonts/hind-bold.ttf')}))

local fonts = {
    body = {font = 'hind', size = 24},
    caption = {font = 'hind', size = 20},
    button = {font = 'hind', size = 23, bold = true},
    heading = {font = 'hind', size = 30, bold = true},
    title = {font = 'hind', size = 40, bold = true},
}

local metrics = {
    controlHeight = 58,
    controlRadius = 29,
    controlPaddingX = 24,
    panelRadius = 22,
    panelPadding = 20,
    cellRadius = 18,
    itemSpacing = 12,
    iconSize = 28,
    chipHeight = 44,
    listRowHeight = 64,
    dialogWidth = 640,
    toastWidth = 560,
    shadowSize = 22,
    shadowOffset = 5,
}

ui.addTheme({
    name = 'storeLight',
    imageFilter = 'linear',
    fonts = fonts,
    metrics = metrics,
    colors = {
        window = '#FFFBF8F4',
        panel = '#FFFBF8F4',
        raised = '#FFFFFFFF',
        border = '#FFEFE6DD',
        borderStrong = '#FFDCCFC3',
        text = '#FF1F2A44',
        textMuted = '#FF6E7488',
        accent = '#FFF2545B',
        accentHover = '#FFF5686E',
        accentStrong = '#FFD9434A',
        accentBackground = '#24F2545B',
        accentText = '#FFD13C44',
        selection = '#2AF2545B',
        focus = '#FF2F86F6',
        track = '#FFEFE6DD',
        scrollbar = '#FFDCCFC3',
        scrollbarHover = '#FFC9BAAD',
        shadow = '#241F2A44',
    },
}, 'light')

ui.addTheme({
    name = 'storeDark',
    imageFilter = 'linear',
    fonts = fonts,
    metrics = metrics,
    colors = {
        window = '#FF14151C',
        panel = '#FF14151C',
        raised = '#FF1E2029',
        border = '#FF2C2F3C',
        borderStrong = '#FF40445A',
        text = '#FFF2EFEA',
        textMuted = '#FFA3A6B5',
        accent = '#FFFF6B71',
        accentHover = '#FFFF8287',
        accentStrong = '#FFE5555B',
        accentBackground = '#33FF6B71',
        accentText = '#FFFF8D92',
        selection = '#3DFF6B71',
        focus = '#FF7AB0FF',
        track = '#FF2C2F3C',
        scrollbar = '#FF363948',
        scrollbarHover = '#FF4A4E62',
    },
}, 'dark')

-- The interface keeps its size in points on every screen, and a television, seen from the couch, draws it half as large again.
if system.info().deviceKind == 'tv' then
    ui.setScale(1.5)
end
ui.setTheme(preferences.get('dark', system.theme() == 'dark') and 'storeDark' or 'storeLight')

events.on('notice', function(text)
    Shell.notify(text, 'information')
end)

Shell.start()

if haylen.platform == 'headless' then
    Tour.start()
end
