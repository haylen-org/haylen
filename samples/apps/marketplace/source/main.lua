-- Marketplace: a local market of second-hand goods in a made-up city, with twelve thousand listings in a grid and on a map, galleries, offers, sellers with reviews, an inbox of chats and a form to sell, in a fresh teal look with a light and a dark theme.
local assets = require('haylen.assets')
local graphics = require('haylen.graphics')
local haylen = require('haylen')
local preferences = require('haylen.preferences')
local system = require('haylen.system')
local ui = require('haylen.ui')

local Shell = require('shell')
local Tour = require('tour')

ui.addFont('prompt', graphics.newFontFamily({regular = assets.font('fonts/prompt-regular.ttf'), bold = assets.font('fonts/prompt-semibold.ttf')}))

local fonts = {
    body = {font = 'prompt', size = 23},
    caption = {font = 'prompt', size = 19},
    button = {font = 'prompt', size = 22, bold = true},
    heading = {font = 'prompt', size = 29, bold = true},
    title = {font = 'prompt', size = 38, bold = true},
}

local metrics = {
    controlHeight = 58,
    controlRadius = 14,
    controlPaddingX = 20,
    panelRadius = 20,
    panelPadding = 20,
    cellRadius = 18,
    itemSpacing = 12,
    iconSize = 28,
    chipHeight = 44,
    listRowHeight = 64,
    dialogWidth = 640,
    toastWidth = 560,
    shadowSize = 20,
    shadowOffset = 4,
    badgePaddingX = 10,
}

ui.addTheme({
    name = 'marketLight',
    imageFilter = 'linear',
    fonts = fonts,
    metrics = metrics,
    colors = {
        window = '#FFF4F7F6',
        panel = '#FFF4F7F6',
        raised = '#FFFFFFFF',
        border = '#FFDFE7E3',
        borderStrong = '#FFC6D3CD',
        text = '#FF14211D',
        textMuted = '#FF62726C',
        accent = '#FF0E9384',
        accentHover = '#FF13A493',
        accentStrong = '#FF0B776B',
        accentBackground = '#240E9384',
        accentText = '#FF0B7A6E',
        selection = '#2A0E9384',
        focus = '#FF2F86F6',
        track = '#FFDCE5E1',
        scrollbar = '#FFC6D3CD',
        scrollbarHover = '#FFAFC0B8',
        warning = '#FFF2A900',
        warningBackground = '#33FFC93C',
        warningText = '#FF8A5A00',
        shadow = '#2410201C',
    },
}, 'light')

ui.addTheme({
    name = 'marketDark',
    imageFilter = 'linear',
    fonts = fonts,
    metrics = metrics,
    colors = {
        window = '#FF101816',
        panel = '#FF101816',
        raised = '#FF19231F',
        border = '#FF26332E',
        borderStrong = '#FF3A4A44',
        text = '#FFE7F0EC',
        textMuted = '#FF8FA39B',
        accent = '#FF19B39F',
        accentHover = '#FF22C5B0',
        accentStrong = '#FF12907F',
        accentBackground = '#3319B39F',
        accentText = '#FF5FD6C4',
        selection = '#3D19B39F',
        focus = '#FF7AB0FF',
        track = '#FF26332E',
        scrollbar = '#FF33433D',
        scrollbarHover = '#FF465850',
        warning = '#FFFFC93C',
    },
}, 'dark')

-- The interface keeps its size in points on every screen, and a television, seen from the couch, draws it half as large again.
if system.info().deviceKind == 'tv' then
    ui.setScale(1.5)
end
ui.setTheme(preferences.get('dark', system.theme() == 'dark') and 'marketDark' or 'marketLight')

Shell.start()

if haylen.platform == 'headless' then
    Tour.start()
end
