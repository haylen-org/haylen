-- One node in another look: a style that replaces colors, metrics, fonts and surfaces for a subtree, styles that nest, subtrees drawn with other registered themes in every state, and the mouse cursor of each node.
local haylen = require('haylen')
local ui = require('haylen.ui')

local Test = require('harness.test')
local layout = require('categories.interface.layout')

local ThemeOverrides = haylen.class('ThemeOverrides', Test)

ThemeOverrides.accents = {
    {id = 'green', text = 'Green', accent = '#FF2E9E62', hover = '#FF3DBE7A', strong = '#FF23804E'},
    {id = 'coral', text = 'Coral', accent = '#FFE0604A', hover = '#FFF07A64', strong = '#FFB84A38'},
    {id = 'violet', text = 'Violet', accent = '#FF8A5CE0', hover = '#FFA27AF0', strong = '#FF6E44C0'},
}
ThemeOverrides.cursors = {'pointingHand', 'iBeam', 'crosshair', 'resizeHorizontal', 'resizeAll', 'notAllowed'}
ThemeOverrides.wood = {image = 'interface/themes/parchment.png', source = {144, 0, 32, 32}, slice = 10, scale = 2}
ThemeOverrides.woodHover = {image = 'interface/themes/parchment.png', source = {178, 0, 32, 32}, slice = 10, scale = 2}

function ThemeOverrides:init(entry)
    ThemeOverrides.super.init(self, entry)
    self.radius = 36
    self.accent = ThemeOverrides.accents[1]
end

function ThemeOverrides:enter()
    ui.loadTheme('interface/themes/parchment.json', 'light')
    self:frame{
        hint = 'The radius slider and the accent control restyle the first group at once, while the buttons around it keep the theme. Hover the cursor buttons with a mouse to see their shapes.',
        focus = 'radius',
        content = {ui.scroll{grow = 1, height = 0, self:columns()}},
    }
    self:apply()
end

function ThemeOverrides:columns()
    local accents = {}
    for index, accent in ipairs(ThemeOverrides.accents) do
        accents[index] = {id = accent.id, text = accent.text}
    end
    local cursors = {}
    for index, name in ipairs(ThemeOverrides.cursors) do
        cursors[index] = ui.button{text = name:sub(1, 1):upper() .. name:sub(2), cursor = name, grow = 1}
    end
    return layout.columns{
        ui.column{grow = 1, gap = 24,
            layout.section('A style on a row', {
                ui.slider{id = 'radius', value = self.radius, min = 0, max = 40, step = 2, showValue = true, decimals = 0, onChange = function(event)
                    self.radius = event.value
                    self:apply()
                end},
                ui.segmentedControl{id = 'accent', items = accents, selected = self.accent.id, onChange = function(event)
                    for _, accent in ipairs(ThemeOverrides.accents) do
                        if accent.id == event.value then
                            self.accent = accent
                        end
                    end
                    self:apply()
                end},
                ui.row{gap = 12, ui.button{text = 'Theme'}, ui.button{text = 'Theme', variant = 'primary'}},
                ui.row{id = 'styled', gap = 12, wrap = true, lineGap = 12,
                    ui.button{text = 'Styled'},
                    ui.button{text = 'Styled', variant = 'primary'},
                    ui.toggle{text = 'On', checked = true},
                    ui.checkbox{text = 'Checked', checked = true},
                    ui.button{text = 'Disabled', variant = 'primary', enabled = false},
                },
            }),
            layout.section('Styles that nest', {
                ui.column{gap = 12, style = {fonts = {button = {size = 34, bold = true}}, metrics = {controlHeight = 84}},
                    ui.row{gap = 12, ui.button{text = 'Large and bold'}, ui.progress{value = 0.6, grow = 1, tone = 'success'}},
                    ui.row{gap = 12, style = {colors = {success = '#FFF2B23A'}, metrics = {controlHeight = 56, progressHeight = 24}},
                        ui.button{text = 'Short and bold'},
                        ui.progress{value = 0.6, grow = 1, tone = 'success'},
                    },
                },
            }),
            layout.section('Surfaces from a style', {
                ui.row{gap = 12,
                    ui.button{text = 'Wooden', style = {surfaces = {button = ThemeOverrides.wood, buttonHover = ThemeOverrides.woodHover, buttonPressed = ThemeOverrides.wood}}},
                    ui.card{theme = 'parchment', style = {surfaces = {card = false}}, ui.label{text = 'Parchment with a flat card'}},
                },
            }),
        },
        ui.column{grow = 1, gap = 24,
            layout.section('A subtree in the light theme', {self:sampler('light')}),
            layout.section('A subtree in the parchment theme', {self:sampler('parchment')}),
            layout.section('The cursor of each node', {ui.grid{columns = 3, gap = 12, children = cursors}}),
        },
    }
end

-- A card in another theme with controls in their normal, selected, focused and disabled states.
function ThemeOverrides:sampler(theme)
    return ui.card{theme = theme, gap = 12,
        ui.row{gap = 12, ui.button{text = 'Button'}, ui.button{text = 'Primary', variant = 'primary'}, ui.button{text = 'Off', enabled = false}},
        ui.row{gap = 24, ui.checkbox{text = 'Check', checked = true}, ui.toggle{text = 'Toggle', checked = true}, ui.toggle{text = 'Off', enabled = false}},
        ui.row{gap = 12, ui.textField{placeholder = 'A field', grow = 1}, ui.chip{text = 'Chip', selected = true}, ui.badge{text = '7', tone = 'danger', solid = true}},
    }
end

-- Applies the radius and the accent to the styled row and reports them.
function ThemeOverrides:apply()
    local accent = self.accent
    self:set('styled', {style = {
        colors = {accent = accent.accent, accentHover = accent.hover, accentStrong = accent.strong},
        metrics = {controlRadius = self.radius, checkRadius = math.min(self.radius, 20), toggleHeight = 44},
    }})
    self:set('status', {text = string.format('Style "controlRadius" %d, accent %s.', self.radius, accent.accent)})
end

return ThemeOverrides
