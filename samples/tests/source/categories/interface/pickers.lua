-- Combos with placeholders and disabled items, color fields with and without opacity, number fields and sliders, and the values they report.
local haylen = require('haylen')
local ui = require('haylen.ui')

local Test = require('harness.test')
local layout = require('categories.interface.layout')

local Pickers = haylen.class('Pickers', Test)

function Pickers:enter()
    self:frame{
        hint = 'Open a combo or a color field with a click, a tap or Enter. Number fields step with their buttons or take typed digits, and sliders move with a drag or with left and right while focused.',
        focus = 'language',
        content = {self:columns()},
    }
end

function Pickers:report(name)
    return function(event)
        if name == 'flag' then
            event.gui:set('flag-preview', {tint = event.value})
        end
        local value = type(event.value) == 'number' and tostring(event.value) or '"' .. tostring(event.value) .. '"'
        self:set('status', {text = string.format('The %s changed to %s.', name, value)})
    end
end

function Pickers:columns()
    local languages = {{id = 'en', text = 'English'}, {id = 'pt-BR', text = 'Português'}, {id = 'es', text = 'Español', enabled = false}}
    local regions = {{id = 'north', text = 'North coast'}, {id = 'south', text = 'South bay'}, {id = 'peak', text = 'The peak'}}
    return layout.columns{
        ui.column{grow = 1, gap = 24,
            layout.section('Component "combo"', {
                ui.combo{id = 'language', items = languages, selected = 'en', onChange = self:report('language')},
                ui.combo{items = regions, placeholder = 'Pick a region', onChange = self:report('region')},
            }),
            layout.section('Component "colorField"', {
                ui.row{gap = 16, ui.label{text = 'Flag', width = 160}, ui.colorField{value = '#FF2E7D32', alpha = false, grow = 1, onChange = self:report('flag')}},
                ui.row{gap = 16, ui.label{text = 'Glass', width = 160}, ui.colorField{value = '#804FC3F7', grow = 1, onChange = self:report('glass')}},
                ui.row{gap = 16, ui.image{id = 'flag-preview', image = 'interface/icons/flag.png', scale = 3, tint = '#FF2E7D32'}, ui.label{text = 'The flag takes the color of the first field as its tint.', color = 'textMuted', grow = 1}},
            }),
        },
        ui.column{grow = 1, gap = 24,
            layout.section('Component "numberField"', {
                ui.settingsRow{label = 'Players', caption = 'From 1 to 8', ui.numberField{value = 2, min = 1, max = 8, width = 320, onChange = self:report('players')}},
                ui.settingsRow{label = 'Sail area', caption = 'Half steps with one decimal', ui.numberField{value = 12.5, min = 0, max = 40, step = 0.5, decimals = 1, width = 320, onChange = self:report('sail area')}},
                ui.settingsRow{label = 'Depth', caption = 'A negative minimum types on the text keyboard', ui.numberField{value = -20, min = -200, max = 0, step = 10, width = 320, onChange = self:report('depth')}},
            }),
            layout.section('Component "slider"', {
                ui.slider{id = 'volume', value = 0.8, showValue = true, onChange = self:report('volume')},
                ui.slider{value = 30, min = 0, max = 100, step = 10, showValue = true, decimals = 0, onChange = self:report('wind')},
                ui.slider{value = 0.3, enabled = false},
            }),
        },
    }
end

return Pickers
