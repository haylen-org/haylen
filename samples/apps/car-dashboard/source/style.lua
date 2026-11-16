-- The look of the dashboard: the palette of a car at night, the gradients that the gauge, the bars and the backdrop share, the UI theme of the dock and the climate controls, and the drawing helpers, which reuse their tables so a frame builds none.
local graphics = require('haylen.graphics')
local graphics2d = require('haylen.graphics2d')
local ui = require('haylen.ui')

local Style = {}

Style.color = {
    night = '#FF060A14',
    deep = '#FF0A1122',
    panel = '#E60E1628',
    panelEdge = '#FF1E2C4A',
    panelGlow = '#2C3B82F6',
    face = '#FF080D1A',
    track = '#FF16213A',
    text = '#FFF1F5FB',
    muted = '#FF8A9BB8',
    faint = '#FF4A5A78',
    cyan = '#FF22D3EE',
    blue = '#FF3B82F6',
    violet = '#FF8B5CF6',
    magenta = '#FFEC4899',
    orange = '#FFF97316',
    green = '#FF22C55E',
    lime = '#FF4ADE80',
    amber = '#FFF5B83D',
    red = '#FFFF3B4E',
    road = '#FF0C1426',
    lane = '#FF9FB0CC',
    map = '#FF0B1424',
    street = '#FF1C2A44',
    park = '#FF123A2C',
    water = '#FF0E2A4A',
}

-- The sizes of the text in design units, large enough to read on a phone.
Style.type = {small = 26, body = 30, label = 34, value = 44, large = 64, speed = 200}

-- Loads the textures of the gradients once: the arc of the gauge turns from cyan through blue and violet to orange, the battery fills from lime to green, and the backdrop falls from deep blue to the night.
function Style.load()
    local arc = graphics.newImage(256, 256)
    arc:fillGradient({kind = 'conic', angle = math.pi * 0.75, stops = {{0, '#FF22D3EE'}, {0.2, '#FF3B82F6'}, {0.42, '#FF8B5CF6'}, {0.6, '#FFEC4899'}, {0.75, '#FFF97316'}, {1, '#FFF97316'}}})
    Style.arc = graphics.newTexture(arc, {filter = 'linear'})

    local battery = graphics.newImage(256, 4)
    battery:fillGradient({stops = {{0, '#FF16A34A'}, {1, '#FF4ADE80'}}})
    Style.battery = graphics.newTexture(battery, {filter = 'linear'})

    local backdrop = graphics.newImage(4, 256)
    backdrop:fillGradient({angle = math.pi / 2, stops = {{0, '#FF0B1430'}, {0.55, '#FF070C1A'}, {1, '#FF05080F'}}})
    Style.backdrop = graphics.newTexture(backdrop, {filter = 'linear'})

    local glow = graphics.newImage(128, 128)
    glow:fillGradient({kind = 'radial', stops = {{0, '#553B82F6'}, {0.6, '#163B82F6'}, {1, '#003B82F6'}}})
    Style.glow = graphics.newTexture(glow, {filter = 'linear'})
end

-- The theme of the dock and the climate controls: clear buttons on the panels of the dashboard, a violet accent for what is on and a cyan focus ring that a remote and a gamepad follow.
function Style.registerTheme()
    ui.setTheme(ui.addTheme({
        name = 'cockpit',
        imageFilter = 'linear',
        colors = {
            background = Style.color.night,
            window = Style.color.night,
            panel = Style.color.panel,
            raised = '#FF141E36',
            border = Style.color.panelEdge,
            borderStrong = '#FF2C3E66',
            text = Style.color.text,
            textMuted = Style.color.muted,
            hover = '#1FFFFFFF',
            pressed = '#33FFFFFF',
            accent = '#FF7C4DFF',
            accentHover = '#FF9170FF',
            accentStrong = '#FF6236F0',
            accentText = '#FFB49CFF',
            accentBackground = '#407C4DFF',
            onAccent = '#FFFFFFFF',
            focus = Style.color.cyan,
        },
        metrics = {controlHeight = 72, controlRadius = 20, panelRadius = 28, iconSize = 40, controlPaddingX = 18, contentSpacing = 8},
        fonts = {body = {size = Style.type.body}, caption = {size = Style.type.small}, button = {size = Style.type.small}, heading = {size = Style.type.label}, title = {size = Style.type.value}},
    }, 'dark'))
end

-- The reused tables of the drawing helpers.
Style.rect = {0, 0, 0, 0}
Style.shape = {}
Style.textStyle = {anchor = {0, 0}}
Style.vectorOptions = {}

-- Draws a rounded rectangle, an ellipse or a sector of one, filled, outlined, soft or textured.
function Style.drawShape(x, y, width, height, radius, color, options)
    local rect, shape = Style.rect, Style.shape
    rect[1], rect[2], rect[3], rect[4] = x, y, width, height
    shape.kind = options and options.kind or 'rectangle'
    shape.radius = radius
    shape.color = color
    shape.borderWidth = options and options.borderWidth or 0
    shape.borderColor = options and options.borderColor or '#00000000'
    shape.softness = options and options.softness or 0
    shape.texture = options and options.texture or nil
    shape.startAngle = options and options.startAngle or 0
    shape.sweep = options and options.sweep or math.pi * 2
    graphics2d.drawShape(rect, shape)
end

-- The reused options of the shapes the cards draw.
Style.cardEdge = {borderWidth = 2, borderColor = Style.color.panelEdge}
Style.cardShadow = {softness = 40}

-- A card of the dashboard: a soft blue glow, the translucent panel and its thin edge.
function Style.drawCard(x, y, width, height)
    Style.drawShape(x, y + 10, width, height, 32, '#66000000', Style.cardShadow)
    Style.drawShape(x, y, width, height, 32, Style.color.panel, Style.cardEdge)
end

-- Draws text with its anchor, `0` for the left or top and `1` for the right or bottom.
function Style.drawText(text, x, y, size, color, anchorX, anchorY, bold)
    local style = Style.textStyle
    style.size = size
    style.color = color
    style.anchor[1], style.anchor[2] = anchorX or 0, anchorY or 0
    style.bold = bold or false
    graphics2d.drawText(nil, text, x, y, style)
end

-- Returns the width of text drawn at a size.
function Style.measureText(text, size, bold)
    local style = Style.textStyle
    style.size = size
    style.bold = bold or false
    local width = graphics2d.measureText(nil, text, style)
    return width
end

-- Draws a line icon centered on a point, tinted with a color.
function Style.drawIcon(icon, x, y, size, color)
    local options = Style.vectorOptions
    options.width, options.height = size, size
    options.pivotX, options.pivotY = 0.5, 0.5
    options.color = color
    graphics2d.drawVector(icon, x, y, options)
end

return Style
