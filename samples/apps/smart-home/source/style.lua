-- The look of the panel: a warm linen wall with white cards, ink text, coral for heat and alerts, teal for the climate that holds, amber for the light and the sun, and sky blue for cooling, in the soft round shapes of a panel on the wall of a home. It also holds the gradients that the drawings share and the theme of the controls.
local graphics = require('haylen.graphics')
local ui = require('haylen.ui')

local Style = {}

Style.color = {
    linen = '#FFF4F1EC',
    mist = '#FFE9EEF3',
    card = '#FFFFFFFF',
    line = '#FFE6E8EE',
    soft = '#FFF5F6F9',
    ink = '#FF1D2433',
    muted = '#FF6B7385',
    faint = '#FFB7BDC9',
    coral = '#FFFF7A59',
    teal = '#FF14B8A6',
    amber = '#FFFFB547',
    sky = '#FF5AA9F7',
    green = '#FF22A06B',
    red = '#FFE5484D',
    violet = '#FF8B7CF6',
}

Style.space = {xs = 8, s = 12, m = 16, l = 24, xl = 32}
Style.radius = {card = 28, control = 18}

-- The cards of the panel: white with a hairline, round corners and room inside.
Style.card = {colors = {raised = Style.color.card, border = Style.color.line}, metrics = {panelRadius = Style.radius.card, panelPadding = Style.space.l}}

-- Loads the gradients once: the sky of the day, of the dusk and of the night behind the house, the ground and the porch of the camera, and the fill under the curve of the sun.
function Style.load()
    Style.skies = {
        day = Style.gradient(4, 256, {angle = math.pi / 2, stops = {{0, '#FF8EC5F5'}, {0.7, '#FFD7ECFB'}, {1, '#FFF4F1EC'}}}),
        dusk = Style.gradient(4, 256, {angle = math.pi / 2, stops = {{0, '#FF4F5B9C'}, {0.55, '#FFF29E7A'}, {1, '#FFF6D3B0'}}}),
        night = Style.gradient(4, 256, {angle = math.pi / 2, stops = {{0, '#FF0F1630'}, {0.7, '#FF26315A'}, {1, '#FF3A4470'}}}),
    }
    Style.wall = Style.gradient(4, 256, {angle = math.pi / 2, stops = {{0, Style.color.linen}, {1, Style.color.mist}}})
    Style.solarFill = Style.gradient(4, 256, {angle = math.pi / 2, stops = {{0, '#FFFFD08A'}, {1, '#FFFFFFFF'}}})
    Style.glow = Style.gradient(128, 128, {kind = 'radial', stops = {{0, '#CCFFD27A'}, {0.5, '#55FFB547'}, {1, '#00FFB547'}}})
    Style.lens = Style.gradient(128, 128, {kind = 'radial', stops = {{0, '#00000000'}, {0.75, '#00000000'}, {1, '#66000000'}}})
end

function Style.gradient(width, height, gradient)
    local image = graphics.newImage(width, height)
    image:fillGradient(gradient)
    return graphics.newTexture(image, {filter = 'linear'})
end

-- The theme of the controls: the light theme of the engine in the colors of the panel, with round controls a finger finds on a wall.
function Style.registerTheme()
    ui.setTheme(ui.addTheme({
        name = 'home',
        imageFilter = 'linear',
        colors = {
            background = Style.color.linen,
            window = Style.color.linen,
            panel = Style.color.card,
            raised = Style.color.soft,
            border = Style.color.line,
            borderStrong = '#FFD5D9E2',
            text = Style.color.ink,
            textMuted = Style.color.muted,
            accent = Style.color.coral,
            accentHover = '#FFFF8F73',
            accentStrong = '#FFF0603D',
            accentText = '#FFE85D3A',
            accentBackground = '#29FF7A59',
            onAccent = '#FFFFFFFF',
            track = '#FFE3E6ED',
            focus = Style.color.teal,
            selection = '#29FF7A59',
        },
        metrics = {controlRadius = Style.radius.control, panelRadius = Style.radius.card, controlHeight = 64, iconSize = 30},
        fonts = {caption = {size = 19}, body = {size = 22}, button = {size = 21}, heading = {size = 27}, title = {size = 40}},
    }, 'light'))
end

return Style
