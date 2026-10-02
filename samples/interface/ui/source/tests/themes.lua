-- Themes: the built-in dark and light themes and the parchment theme of this sample, whose nine-slice surfaces cut one atlas image into panels, buttons, fields, tracks, tabs, chips and slots.
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local ui = require('haylen.ui')
local viewport = require('haylen.viewport')

local sample = require('sample')

local Themes = haylen.class('Themes', sample.Test)

Themes.hints = 'Switch the theme with the segmented control, or with left and right while it has the focus. Every document restyles at once, the header included, and leaving the test goes back to the dark theme.'
Themes.focus = 'theme'

Themes.backgrounds = {dark = '#FF101418', light = '#FFDDE2EC', parchment = '#FFC9A86E'}

function Themes:describe()
    local body = ui.themeFont('body')
    local surface = ui.themeSurface('button')
    self:setStatus(string.format('Theme %s: "controlHeight" %d, body font %s at %d, buttons drawn %s', ui.theme(), ui.themeMetric('controlHeight'), body.font, body.size, surface and 'with a nine-slice' or 'with flat colors'))
end

function Themes:content()
    local themes = {}
    for index, name in ipairs(ui.themes()) do
        themes[index] = {id = name, text = name:sub(1, 1):upper() .. name:sub(2)}
    end
    local slots = {{id = 'a', image = 'icons/sword.png'}, {id = 'b', image = 'icons/apple.png', count = 3}, {id = 'c'}, {id = 'd', image = 'icons/gem.png'}}
    return ui.column{
        gap = 24,
        ui.row{gap = 24, ui.label{text = 'Theme', font = 'heading'}, ui.segmentedControl{id = 'theme', width = 720, items = themes, selected = ui.theme(), onChange = function(event)
            ui.setTheme(event.value)
            self:describe()
        end}},
        sample.columns{
            ui.panel{grow = 1, gap = 16,
                ui.pageHeader{title = 'Harbor', caption = 'A banner header', banner = true},
                ui.row{gap = 12, ui.button{text = 'Default'}, ui.button{text = 'Primary', variant = 'primary'}, ui.button{text = 'Delete', variant = 'destructive'}},
                ui.row{gap = 24, ui.checkbox{text = 'Check', checked = true}, ui.checkbox{text = 'Box'}},
                ui.toggle{text = 'Toggle', checked = true},
                ui.row{gap = 12, ui.chip{text = 'Chip', selected = true}, ui.chip{text = 'Another'}, ui.badge{text = 'Badge', tone = 'accent'}, ui.badge{text = '3', tone = 'danger', solid = true}},
            },
            ui.card{grow = 1, gap = 16,
                ui.textField{placeholder = 'A text field'},
                ui.combo{items = {{id = 'one', text = 'A combo'}, {id = 'two', text = 'Second item'}}, selected = 'one'},
                ui.slider{value = 0.6, showValue = true},
                ui.progress{value = 0.7, tone = 'success', text = 'Health'},
                ui.stepper{items = {{id = 'easy', text = 'Easy'}, {id = 'hard', text = 'Hard'}}, selected = 'easy'},
            },
            ui.card{grow = 1, gap = 16,
                ui.tabs{items = {{id = 'bag', text = 'Bag'}, {id = 'map', text = 'Map'}}, selected = 'bag',
                    ui.slotGrid{columns = 4, slotSize = 80, slots = slots, selected = 'a'},
                    ui.label{text = 'The map tab'},
                },
                ui.segmentedControl{items = {{id = 'day', text = 'Day'}, {id = 'night', text = 'Night'}}, selected = 'day'},
                ui.row{gap = 12,
                    ui.button{text = 'Dialog', onClick = function(event) event.document:set('dialog', {open = true}) end},
                    ui.button{text = 'Toast', onClick = function(event) event.document:set('toast', {open = true}) end},
                    ui.button{text = 'Tooltip', tooltip = 'Tooltips take the tooltip surface'},
                },
            },
        },
        ui.dialog{id = 'dialog', title = 'Set sail?', message = 'Dialogs take the dialog surface of the theme.', buttons = {{id = 'stay', text = 'Stay'}, {id = 'sail', text = 'Sail', variant = 'primary'}}},
        ui.toast{id = 'toast', text = 'A toast on its own surface', tone = 'success'},
    }
end

function Themes:started()
    self:describe()
end

function Themes:exit()
    ui.setTheme('dark')
end

function Themes:render()
    graphics2d.beginScreen()
    graphics2d.drawRect(viewport.visibleRect(), Themes.backgrounds[ui.theme()])
end

return Themes
