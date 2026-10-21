-- The look of the app as data: the palette, the type scale, the spacing scale and the shapes. The theme takes them and every screen reads them, so text, gaps and corners keep one proportion on every screen. Sizes are UI units of the physical scale, three quarters of a point each.
local assets = require('haylen.assets')
local graphics = require('haylen.graphics')
local ui = require('haylen.ui')

local Style = {}

Style.color = {
    paper = '#FFFAF7F2',
    surface = '#FFFFFFFF',
    sand = '#FFF2EBE1',
    line = '#FFE9E2D8',
    lineStrong = '#FFD8CFC3',
    ink = '#FF1D1B18',
    muted = '#FF776F66',
    green = '#FF2D6A4F',
    sale = '#FFC2452D',
    star = '#FFE0A526',
}

-- The type scale, the sizes of the em square that the UI sizes every font by. Titles and prices are set in Crimson Text, a serif, and everything else in Hind, each step about a quarter larger than the one below it.
Style.type = {caption = 18, body = 22, label = 21, heading = 29, title = 40, display = 53}

-- The spacing scale, in steps of four units, for every gap and padding. The store breathes, so its pages keep wide margins.
Style.space = {xxs = 4, xs = 8, s = 12, m = 16, l = 24, xl = 32, xxl = 48, xxxl = 64}

-- The shapes: soft rectangles everywhere, with pictures a little rounder than controls.
Style.radius = {control = 10, picture = 14, card = 16}

-- The sizes of the parts the screens repeat.
Style.size = {
    header = 84,
    nav = 96,
    tabBar = 92,
    page = 1200,
    reading = 760,
    card = 248,
    tile = 148,
}

-- The width in UI units from which the window shows the navigation at the top and the wide layouts.
Style.wideWidth = 960

-- A style that draws a node in one size of the scale with the font role it names.
function Style.text(role, size)
    return {fonts = {[role] = {size = size}}}
end

function Style.register()
    ui.addFont('hind', graphics.newFontFamily({regular = assets.font('fonts/hind-regular.ttf'), bold = assets.font('fonts/hind-bold.ttf')}))
    ui.addFont('crimson', graphics.newFontFamily({regular = assets.font('fonts/crimson-text-regular.ttf'), bold = assets.font('fonts/crimson-text-bold.ttf'), italic = assets.font('fonts/crimson-text-italic.ttf')}))
    local color, type, space, radius = Style.color, Style.type, Style.space, Style.radius
    ui.addTheme({
        name = 'store',
        imageFilter = 'linear',
        fonts = {
            body = {font = 'hind', size = type.body},
            caption = {font = 'hind', size = type.caption},
            button = {font = 'hind', size = type.label, bold = true},
            heading = {font = 'crimson', size = type.heading, bold = true},
            title = {font = 'crimson', size = type.title, bold = true},
        },
        metrics = {
            controlHeight = 60,
            controlRadius = radius.control,
            controlPaddingX = space.l,
            controlPaddingY = space.s,
            itemSpacing = space.s,
            panelPadding = space.l,
            panelRadius = radius.card,
            cellRadius = radius.picture,
            borderWidth = 1.5,
            iconSize = 28,
            chipHeight = 46,
            listRowHeight = 72,
            rowPadding = space.m,
            contentSpacing = space.s,
            checkRadius = 4,
            badgePaddingX = 10,
            dialogWidth = 680,
            toastWidth = 620,
            shadowSize = 18,
            shadowOffset = 4,
            focusWidth = 3,
        },
        colors = {
            window = color.paper,
            panel = color.paper,
            raised = color.surface,
            border = color.line,
            borderStrong = color.lineStrong,
            text = color.ink,
            textMuted = color.muted,
            accent = color.ink,
            accentHover = '#FF38342F',
            accentStrong = '#FF000000',
            onAccent = color.surface,
            accentBackground = '#141D1B18',
            accentText = color.green,
            selection = '#121D1B18',
            focus = color.green,
            caret = color.ink,
            success = color.green,
            successText = color.green,
            successBackground = '#1A2D6A4F',
            danger = color.sale,
            dangerText = '#FFB03A24',
            dangerBackground = '#1AC2452D',
            warning = color.star,
            onWarning = color.ink,
            information = color.ink,
            onInformation = color.surface,
            informationText = color.ink,
            informationBackground = '#141D1B18',
            track = color.lineStrong,
            scrollbar = color.lineStrong,
            scrollbarHover = '#FFC2B8AB',
            tooltip = color.ink,
            onTooltip = color.surface,
            shadow = '#241D1B18',
        },
    }, 'light')
end

return Style
