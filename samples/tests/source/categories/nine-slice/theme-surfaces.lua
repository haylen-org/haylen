-- UI theme surfaces: a theme whose panels, banner, buttons, tracks, fills and knobs are nine-slice images. Every piece is one region of the near white atlas `nine-slice/theme.png`, painted at twice the size it draws at, and the pieces take the color of each component through `colorize`, so one fill paints every tone. The theme applies to the whole screen while the test runs.
local haylen = require('haylen')
local ui = require('haylen.ui')

local Test = require('harness.test')

local ThemeSurfaces = haylen.class('ThemeSurfaces', Test)

ThemeSurfaces.columnWidth = 440
ThemeSurfaces.tones = {'accent', 'success', 'warning', 'danger'}
ThemeSurfaces.code = [[
local atlas = 'nine-slice/theme.png'
local button = {image = atlas, source = {0, 0, 96, 96}, slice = 32, scale = 0.5, colorize = true}
local fill = {image = atlas, source = {324, 184, 48, 36}, slice = 18, scale = 0.5, colorize = true}
local theme = ui.addTheme({name = 'workshop', surfaces = {button = button, trackFill = fill}}, 'light')
ui.setTheme(theme)  -- One near white fill becomes the accent, success, warning and danger bars.]]

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
            background = '#FFE9DCC0', window = '#FFFBF3E1', panel = '#FFF2E3C6', raised = '#FFFBF3E1', text = '#FF3A2A1E', textMuted = '#FF6E5A45',
            accent = '#FF3E8BA3', accentHover = '#FF4F9DB5', accentStrong = '#FF2C6F86', accentText = '#FF2C6F86',
            danger = '#FFD9534A', success = '#FF6CBF4F', warning = '#FFF0A63C', border = '#FF6E5A45', borderStrong = '#FF3A2A1E', focus = '#FFF2C14E',
        },
        metrics = {controlHeight = 68, sliderTrackHeight = 24, sliderKnobSize = 40, progressHeight = 30, toggleWidth = 84, toggleHeight = 40},
        surfaces = {
            panel = {image = atlas, source = regions.panel, slice = 40, scale = 0.5, padding = 10, tint = '#FFFFF3DC'},
            card = {image = atlas, source = regions.panel, slice = 40, scale = 0.5, padding = 10, tint = '#FFFFF8EC'},
            banner = {image = atlas, source = regions.banner, slice = {0, 80}, scale = 0.5, padding = {16, 44, 46, 44}, colorize = colorize},
            button = button(regions.button, {0, 0, 4, 0}),
            buttonPressed = button(regions.buttonPressed, {4, 0, 0, 0}),
            buttonPrimary = button(regions.button, {0, 0, 4, 0}),
            buttonPrimaryPressed = button(regions.buttonPressed, {4, 0, 0, 0}),
            buttonDestructive = button(regions.button, {0, 0, 4, 0}),
            buttonDestructivePressed = button(regions.buttonPressed, {4, 0, 0, 0}),
            track = {image = atlas, source = regions.track, slice = 24, scale = 0.5, padding = 5},
            trackFill = {image = atlas, source = regions.fill, slice = 18, scale = 0.5, colorize = colorize},
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
        content = {self:demo()},
    }
end

-- The workshop: the banner over the buttons and a slider on the left, and on the right the bars of the four tones over the near white atlas that every piece comes from.
function ThemeSurfaces:demo()
    local bars = {}
    for index, tone in ipairs(ThemeSurfaces.tones) do
        bars[index] = ui.row{gap = 12, alignItems = 'center', ui.label{text = tone:sub(1, 1):upper() .. tone:sub(2), width = 130}, ui.progress{id = tone, value = 0.6, tone = tone, grow = 1}}
    end
    return ui.row{wrap = true, gap = 24, alignItems = 'start',
        ui.column{grow = 1, minWidth = ThemeSurfaces.columnWidth, gap = 16,
            ui.pageHeader{title = 'Workshop', caption = 'Every surface is a nine-slice', banner = true, textAlign = 'center'},
            ui.panel{gap = 18,
                ui.row{wrap = true, gap = 12,
                    ui.button{id = 'default', text = 'Default', grow = 1},
                    ui.button{id = 'accept', text = 'Primary', variant = 'primary', grow = 1},
                    ui.button{id = 'delete', text = 'Destructive', variant = 'destructive', grow = 1},
                },
                ui.slider{id = 'volume', value = 0.5, showValue = true},
            },
        },
        ui.column{grow = 1, minWidth = ThemeSurfaces.columnWidth, gap = 16,
            ui.panel{gap = 10, ui.sectionTitle{text = 'One fill in four tones'}, ui.column{gap = 10, children = bars}},
            ui.card{gap = 10,
                ui.sectionTitle{text = 'The near white pieces of "nine-slice/theme.png"'},
                ui.image{image = ThemeSurfaces.atlas, height = 150, fit = 'contain', align = 'stretch'},
            },
        },
    }
end

function ThemeSurfaces:exit()
    ui.setTheme(self.previousTheme)
    ThemeSurfaces.super.exit(self)
end

function ThemeSurfaces:fill(value)
    for _, tone in ipairs(ThemeSurfaces.tones) do
        self.gui:set(tone, {value = value})
    end
end

function ThemeSurfaces:update(dt)
    ThemeSurfaces.super.update(self, dt)
    local fill = ui.themeSurface('trackFill')
    self:status(string.format('The theme "%s" draws its fills with borders of %g pixels, %s.', ui.theme(), fill.slice.borders[1], fill.colorize and 'colored by each component' or 'as they are painted'))
end

return ThemeSurfaces
