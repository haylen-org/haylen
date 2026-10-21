-- The look of the app as data: the palette, the type scale, the spacing scale and the shapes. The theme takes them and every screen reads them, so text, gaps and corners keep one proportion on every screen. Sizes are UI units of the physical scale, three quarters of a point each.
local assets = require('haylen.assets')
local graphics = require('haylen.graphics')
local ui = require('haylen.ui')

local Style = {}

Style.color = {
    night = '#FF0B0A12',
    raised = '#FF17151F',
    lifted = '#FF221F2D',
    line = '#FF2A2636',
    lineStrong = '#FF3B364C',
    text = '#FFF5F3FF',
    muted = '#FFA19DB5',
    violet = '#FF8B5CFF',
    violetLight = '#FFB89EFF',
    pink = '#FFFF4D9D',
    yellow = '#FFFFD24A',
}

-- The type scale, the sizes of the em square that the UI sizes every font by: large and heavy headings over a quieter body, each step about a third larger than the one below it.
Style.type = {caption = 18, body = 22, label = 21, heading = 31, title = 43, display = 59}

-- The spacing scale, in steps of four units, for every gap and padding.
Style.space = {xxs = 4, xs = 8, s = 12, m = 16, l = 24, xl = 32, xxl = 48}

-- The shapes: round controls and pills against covers with small corners.
Style.radius = {cover = 8, card = 14, pill = 30}

-- The sizes of the parts the screens repeat.
Style.size = {
    header = 84,
    tabBar = 88,
    sidebar = 300,
    playerBar = 112,
    cover = 200,
    artist = 170,
    page = 1400,
}

-- The width in UI units from which the window shows the sidebar and the player bar of a desktop.
Style.wideWidth = 960

-- A style that draws a node in one size of the scale with the font role it names.
function Style.text(role, size)
    return {fonts = {[role] = {size = size}}}
end

function Style.register()
    ui.addFont('kanit', graphics.newFontFamily({regular = assets.font('fonts/kanit-regular.ttf'), bold = assets.font('fonts/kanit-semibold.ttf')}))
    local color, type, space, radius = Style.color, Style.type, Style.space, Style.radius
    ui.addTheme({
        name = 'music',
        imageFilter = 'linear',
        fonts = {
            body = {font = 'kanit', size = type.body},
            caption = {font = 'kanit', size = type.caption},
            button = {font = 'kanit', size = type.label, bold = true},
            heading = {font = 'kanit', size = type.heading, bold = true},
            title = {font = 'kanit', size = type.title, bold = true},
        },
        metrics = {
            controlHeight = 60,
            controlRadius = radius.pill,
            controlPaddingX = space.l,
            controlPaddingY = space.s,
            itemSpacing = space.s,
            panelPadding = space.l,
            panelRadius = radius.card,
            cellRadius = radius.card,
            borderWidth = 1.5,
            iconSize = 30,
            chipHeight = 48,
            listRowHeight = 76,
            rowPadding = space.m,
            contentSpacing = space.s,
            badgePaddingX = 10,
            dialogWidth = 640,
            toastWidth = 600,
            sliderTrackHeight = 6,
            sliderKnobSize = 22,
            shadowSize = 26,
            shadowOffset = 6,
            focusWidth = 3,
        },
        colors = {
            window = color.night,
            panel = color.raised,
            raised = color.raised,
            border = color.line,
            borderStrong = color.lineStrong,
            text = color.text,
            textMuted = color.muted,
            accent = color.violet,
            accentHover = '#FF9C74FF',
            accentStrong = '#FF7445F0',
            onAccent = '#FFFFFFFF',
            accentBackground = '#338B5CFF',
            accentText = color.violetLight,
            selection = '#338B5CFF',
            hover = '#14FFFFFF',
            pressed = '#24FFFFFF',
            focus = color.yellow,
            caret = color.violetLight,
            track = '#FF3B364C',
            scrollbar = color.lineStrong,
            scrollbarHover = '#FF524B68',
            tooltip = color.lifted,
            onTooltip = color.text,
            shadow = '#A6000000',
            overlay = '#B3000000',
        },
    }, 'dark')
end

return Style
