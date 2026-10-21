-- The look of the app as data: the palette, the type scale, the spacing scale and the shapes. The theme takes them and every screen reads them, so text, gaps and corners keep one proportion on every screen. Sizes are UI units of the physical scale, three quarters of a point each.
local assets = require('haylen.assets')
local graphics = require('haylen.graphics')
local ui = require('haylen.ui')

local Style = {}

Style.color = {
    paper = '#FFFFFFFF',
    mist = '#FFF4F4F7',
    line = '#FFEBEBF0',
    lineStrong = '#FFD6D6DE',
    ink = '#FF141418',
    muted = '#FF71717E',
    blue = '#FF2F6BFF',
    blueDeep = '#FF2153D6',
    heart = '#FFFF3358',
}

-- The type scale, the sizes of the em square that the UI sizes every font by: each step is about a quarter larger than the one below it, from the captions of times and counts up to the wordmark.
Style.type = {caption = 18, body = 21, label = 21, heading = 26, title = 35, display = 43}

-- The spacing scale, in steps of four units, for every gap and padding.
Style.space = {xxs = 4, xs = 8, s = 12, m = 16, l = 24, xl = 32, xxl = 48}

-- The shapes: photos are prints with soft corners, the tiles of a grid a little less round, controls are rounded squares and fields and bubbles are rounder.
Style.radius = {photo = 16, tile = 10, control = 14, field = 26, bubble = 26, card = 24}

-- The sizes of the parts the screens repeat.
Style.size = {
    avatar = 56,
    avatarSmall = 44,
    story = 112,
    icon = 30,
    tabBar = 84,
    header = 88,
    rail = 300,
    column = 640,
    side = 360,
}

-- The width in UI units from which the window shows the rail and the wide layouts.
Style.wideWidth = 960
-- The width from which the feed also shows the column of suggestions.
Style.roomyWidth = 1460

-- A style that draws a node in one size of the scale with the font role it names.
function Style.text(role, size)
    return {fonts = {[role] = {size = size}}}
end

function Style.register()
    ui.addFont('poppins', graphics.newFontFamily({regular = assets.font('fonts/poppins-regular.ttf'), bold = assets.font('fonts/poppins-semibold.ttf')}))
    local color, type, space, radius = Style.color, Style.type, Style.space, Style.radius
    ui.addTheme({
        name = 'photos',
        imageFilter = 'linear',
        fonts = {
            body = {font = 'poppins', size = type.body},
            caption = {font = 'poppins', size = type.caption},
            button = {font = 'poppins', size = type.label, bold = true},
            heading = {font = 'poppins', size = type.heading, bold = true},
            title = {font = 'poppins', size = type.title, bold = true},
        },
        metrics = {
            controlHeight = 60,
            controlRadius = radius.control,
            controlPaddingX = space.l,
            controlPaddingY = space.s,
            itemSpacing = space.s,
            panelPadding = space.l,
            panelRadius = radius.card,
            cellRadius = radius.control,
            borderWidth = 1.5,
            iconSize = Style.size.icon,
            chipHeight = 48,
            listRowHeight = 72,
            rowPadding = space.m,
            contentSpacing = space.s,
            badgePaddingX = 10,
            dialogWidth = 640,
            toastWidth = 600,
            shadowSize = 22,
            shadowOffset = 4,
            focusWidth = 3,
        },
        colors = {
            window = color.paper,
            panel = color.paper,
            raised = color.paper,
            border = color.line,
            borderStrong = color.lineStrong,
            text = color.ink,
            textMuted = color.muted,
            accent = color.blue,
            accentHover = '#FF4A80FF',
            accentStrong = color.blueDeep,
            accentBackground = '#1F2F6BFF',
            accentText = color.blueDeep,
            selection = '#1A2F6BFF',
            focus = color.blue,
            caret = color.blue,
            danger = color.heart,
            dangerText = '#FFE0213F',
            track = color.lineStrong,
            scrollbar = color.lineStrong,
            scrollbarHover = '#FFB9B9C4',
            tooltip = color.ink,
            onTooltip = color.paper,
            shadow = '#29141418',
        },
    }, 'light')
end

return Style
