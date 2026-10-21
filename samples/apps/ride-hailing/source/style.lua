-- The look of the app as data: the palette of the screens and of the map, the type scale, the spacing scale and the shapes. The theme takes them and every screen reads them, so text, gaps and corners keep one proportion on every screen. Sizes are UI units of the physical scale, three quarters of a point each.
local assets = require('haylen.assets')
local graphics = require('haylen.graphics')
local ui = require('haylen.ui')

local Style = {}

Style.color = {
    paper = '#FFFFFFFF',
    mist = '#FFF2F2EF',
    line = '#FFE5E3DD',
    lineStrong = '#FFCFCCC4',
    ink = '#FF15161A',
    muted = '#FF686A72',
    yellow = '#FFFFC629',
    yellowDeep = '#FFE8AE0C',
    green = '#FF1E9E5A',
    red = '#FFE0443A',
}

-- The day map: warm ground, white streets, pale yellow avenues, green parks and blue water, which every map of the app draws.
Style.map = {
    ground = '#FFE9E6DC',
    road = '#FFFFFFFF',
    roadEdge = '#FFD9D4C8',
    avenue = '#FFFFEBAE',
    block = '#FFF3F1EA',
    buildings = {'#FFE2DED3', '#FFDCD7CA', '#FFE7E3D9', '#FFD6D1C3'},
    roof = '#FFC9C3B4',
    park = '#FFC6E6B2',
    tree = '#FF8FCB73',
    treeDark = '#FF74B35A',
    water = '#FFA9D9EF',
    waterEdge = '#FF8BC9E4',
    bridge = '#FFFFFFFF',
}

-- The type scale, in points of the em square, the size people mean by a font size: clear and compact, with large numbers for times and prices, each step about a third larger than the one below it.
Style.points = {caption = 14, body = 17, label = 17, heading = 24, title = 32, display = 48}
-- A UI unit spans three quarters of a point, and the UI sizes a font by its height from ascent to descent, which Barlow draws in 1.2 em, so the sizes of the theme are the points of the scale turned into that height.
Style.pointsPerUnit = 0.75
Style.fontHeight = 1.2
Style.type = {}
for name, points in pairs(Style.points) do
    Style.type[name] = math.floor(points / Style.pointsPerUnit * Style.fontHeight + 0.5)
end

-- The spacing scale, in steps of four units, for every gap and padding.
Style.space = {xxs = 4, xs = 8, s = 12, m = 16, l = 24, xl = 32, xxl = 48}

-- The shapes: sheets with large round corners and rounded square controls, while the buttons over the map are circles.
Style.radius = {sheet = 28, control = 16, chip = 12}

-- The sizes of the parts the screens repeat.
Style.size = {
    header = 84,
    mapButton = 64,
    panel = 540,
    reading = 820,
}

-- The width in UI units from which the window shows the sheets as a panel beside the map instead of at the bottom.
Style.wideWidth = 960

-- A style that draws a node in one size of the scale with the font role it names.
function Style.text(role, size)
    return {fonts = {[role] = {size = size}}}
end

function Style.register()
    ui.addFont('barlow', graphics.newFontFamily({regular = assets.font('fonts/barlow-regular.ttf'), bold = assets.font('fonts/barlow-semibold.ttf')}))
    local color, type, space, radius = Style.color, Style.type, Style.space, Style.radius
    ui.addTheme({
        name = 'rides',
        imageFilter = 'linear',
        fonts = {
            body = {font = 'barlow', size = type.body},
            caption = {font = 'barlow', size = type.caption},
            button = {font = 'barlow', size = type.label, bold = true},
            heading = {font = 'barlow', size = type.heading, bold = true},
            title = {font = 'barlow', size = type.title, bold = true},
        },
        metrics = {
            controlHeight = 64,
            controlRadius = radius.control,
            controlPaddingX = space.l,
            controlPaddingY = space.s,
            itemSpacing = space.s,
            panelPadding = space.l,
            panelRadius = radius.sheet,
            cellRadius = radius.control,
            borderWidth = 1.5,
            iconSize = 30,
            chipHeight = 48,
            listRowHeight = 76,
            rowPadding = space.m,
            contentSpacing = space.s,
            badgePaddingX = 12,
            dialogWidth = 640,
            toastWidth = 620,
            progressHeight = 8,
            shadowSize = 28,
            shadowOffset = 6,
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
            accent = color.yellow,
            accentHover = '#FFFFD04D',
            accentStrong = color.yellowDeep,
            onAccent = color.ink,
            accentBackground = '#33FFC629',
            accentText = color.ink,
            selection = '#38FFC629',
            focus = color.ink,
            caret = color.ink,
            success = color.green,
            successText = '#FF15804A',
            danger = color.red,
            information = color.ink,
            onInformation = color.paper,
            track = color.line,
            scrollbar = color.lineStrong,
            scrollbarHover = '#FFB7B3A9',
            tooltip = color.ink,
            onTooltip = color.paper,
            shadow = '#3315161A',
        },
    }, 'light')
end

return Style
