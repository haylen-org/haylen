-- UI theme surfaces: a theme whose panels, banner, buttons, tracks, fills and knobs are nine-slice images. Every piece is one region of the near white atlas `nine-slice/theme.png`, painted at twice the size it draws at, and the pieces take the color of each component through `colorize`, so one fill paints every tone. The theme applies to the whole screen while the test runs.
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local ui = require('haylen.ui')
local viewport = require('haylen.viewport')

local Test = require('harness.test')

local ThemeSurfaces = haylen.class('ThemeSurfaces', Test)

ThemeSurfaces.demoWidth = 640
ThemeSurfaces.tones = {'accent', 'success', 'warning', 'danger'}
ThemeSurfaces.code = [[
ui.setTheme(ui.addTheme({name = 'workshop', surfaces = {
    button = {image = 'nine-slice/theme.png', source = {0, 0, 96, 96}, slice = 32, scale = 0.5, padding = {0, 0, 4, 0}, colorize = true},
    trackFill = {image = 'nine-slice/theme.png', source = {324, 184, 48, 36}, slice = 14, scale = 0.5, colorize = true},
}}, 'light'))  -- One near white fill becomes the accent, success, warning and danger bars.]]

-- The regions of the pieces in the atlas.
ThemeSurfaces.atlas = 'nine-slice/theme.png'
ThemeSurfaces.regions = {
    button = {0, 0, 96, 96}, buttonPressed = {100, 0, 96, 96}, panel = {200, 0, 128, 128}, knob = {332, 0, 80, 80},
    banner = {0, 132, 320, 112}, track = {324, 132, 96, 48}, fill = {324, 184, 48, 36},
}

-- The theme document, with `colorize` on or off for the surfaces that take the color of their component.
function ThemeSurfaces.theme(colorize)
    local atlas, regions = ThemeSurfaces.atlas, ThemeSurfaces.regions
    local button = function(source, padding) return {image = atlas, source = source, slice = 32, scale = 0.5, padding = padding, colorize = colorize} end
    return {
        name = 'workshop',
        colors = {
            window = '#FFFBF3E1', panel = '#FFF2E3C6', raised = '#FFFBF3E1', text = '#FF3A2A1E', textMuted = '#FF6E5A45',
            accent = '#FF3E8BA3', accentHover = '#FF4F9DB5', accentStrong = '#FF2C6F86', accentText = '#FF2C6F86',
            danger = '#FFD9534A', success = '#FF6CBF4F', warning = '#FFF0A63C', border = '#FF6E5A45', borderStrong = '#FF3A2A1E', focus = '#FFF2C14E',
        },
        metrics = {controlHeight = 68, sliderTrackHeight = 24, sliderKnobSize = 40, progressHeight = 30, toggleWidth = 84, toggleHeight = 40},
        surfaces = {
            panel = {image = atlas, source = regions.panel, slice = 40, scale = 0.5, padding = 10, tint = '#FFFFF3DC'},
            card = {image = atlas, source = regions.panel, slice = 40, scale = 0.5, padding = 10, tint = '#FFFFF8EC'},
            banner = {image = atlas, source = regions.banner, slice = {20, 80}, scale = 0.5, padding = {8, 44}, colorize = colorize},
            button = button(regions.button, {0, 0, 4, 0}),
            buttonPressed = button(regions.buttonPressed, {4, 0, 0, 0}),
            buttonPrimary = button(regions.button, {0, 0, 4, 0}),
            buttonPrimaryPressed = button(regions.buttonPressed, {4, 0, 0, 0}),
            buttonDestructive = button(regions.button, {0, 0, 4, 0}),
            buttonDestructivePressed = button(regions.buttonPressed, {4, 0, 0, 0}),
            track = {image = atlas, source = regions.track, slice = 22, scale = 0.5, padding = 5},
            trackFill = {image = atlas, source = regions.fill, slice = 14, scale = 0.5, colorize = colorize},
            knob = {image = atlas, source = regions.knob, scale = 0.5},
        },
    }
end

function ThemeSurfaces:enter()
    self.previousTheme = ui.theme()
    ui.setTheme(ui.addTheme(ThemeSurfaces.theme(true), 'light'))
    self:frame{
        code = ThemeSurfaces.code,
        hint = 'Turn "colorize" off to see the near white pieces as they are.',
        controls = {
            ui.toggle{id = 'colorize', text = 'Colorize', checked = true, onChange = function(event) ui.addTheme(ThemeSurfaces.theme(event.checked), 'light') end},
            ui.formField{label = 'Progress', ui.slider{id = 'progress', min = 0, max = 1, value = 0.6, showValue = true, onChange = function(event) self:fill(event.value) end}},
            ui.button{id = 'primary', text = 'Primary', variant = 'primary'},
        },
        focus = 'colorize',
    }

    local bars = {}
    for index, tone in ipairs(ThemeSurfaces.tones) do
        bars[index] = ui.progress{id = tone, value = 0.6, tone = tone, text = 'Tone "' .. tone .. '"'}
    end
    self.demo = ui.mount(ui.column{id = 'demo', anchor = 'topLeft', width = ThemeSurfaces.demoWidth, gap = 16, onCancel = function() self:cancel() end,
        ui.pageHeader{title = 'Workshop', caption = 'Every surface is a nine-slice', banner = true, textAlign = 'center'},
        ui.panel{gap = 14,
            ui.row{wrap = true, gap = 12,
                ui.button{id = 'default', text = 'Default', grow = 1},
                ui.button{id = 'accept', text = 'Primary', variant = 'primary', grow = 1},
                ui.button{id = 'delete', text = 'Destructive', variant = 'destructive', grow = 1},
            },
            ui.column{gap = 10, children = bars},
            ui.slider{id = 'volume', value = 0.5, showValue = true},
        },
    }, {owner = self, layer = 1})
end

function ThemeSurfaces:exit()
    ui.setTheme(self.previousTheme)
    ThemeSurfaces.super.exit(self)
end

function ThemeSurfaces:fill(value)
    for _, tone in ipairs(ThemeSurfaces.tones) do
        self.demo:set(tone, {value = value})
    end
end

-- Places the demo in the left part of the play area, which the safe area and the frame decide.
function ThemeSurfaces:resize(area)
    local safe = viewport.safeRect()
    self.demo:set('demo', {margin = {self.stage.y - safe.y + 20, 0, 0, self.stage.x - safe.x + 20}})
end

function ThemeSurfaces:update(dt)
    ThemeSurfaces.super.update(self, dt)
    local fill = ui.themeSurface('trackFill')
    self:status(string.format('Theme "%s"   Borders of "trackFill" %s   Property "colorize" is "%s"', ui.theme(), table.concat(fill.slice.borders, ', '), fill.colorize))
end

-- The theme writes dark text, so the page behind the frame is light while the test runs.
function ThemeSurfaces:render()
    graphics2d.beginScreen({order = -1})
    graphics2d.drawRect(viewport.visibleRect(), '#FFE9DCC0')
    ThemeSurfaces.super.render(self)
end

-- Draws the fill of the theme once per tone, multiplied by the color of the tone as `colorize` does, with the borders at the scale of the surface.
function ThemeSurfaces:draw(area)
    local left = ThemeSurfaces.demoWidth + 70
    local fill, button = ui.themeSurface('trackFill'), ui.themeSurface('button')
    local width = area.width - left - 40
    Test.caption('The fill of "nine-slice/theme.png" times each tone', left, 30, {size = 24, color = Test.ink, maxWidth = width})
    for index, tone in ipairs(ThemeSurfaces.tones) do
        graphics2d.drawNineSlice(fill.slice, {left, 50 + index * 46, width, 30}, ui.themeColor(tone), {borderScale = fill.scale})
    end
    Test.caption('The button of "nine-slice/theme.png" times "raised", "accent" and "danger"', left, 300, {size = 24, color = Test.ink, maxWidth = width})
    local third = (width - 20) / 3
    for index, role in ipairs({'raised', 'accent', 'danger'}) do
        graphics2d.drawNineSlice(button.slice, {left + (index - 1) * (third + 10), 370, third, 64}, ui.themeColor(role), {borderScale = button.scale})
    end
end

return ThemeSurfaces
