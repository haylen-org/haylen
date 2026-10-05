-- A key whose table holds `zero`, `one` and `other` texts is a plural form, and the `count` argument picks the text: `zero` for 0 and `one` for 1 when the form has them, `other` for everything else. Japanese counts without a singular, so its forms have no `one` text.
local haylen = require('haylen')
local localization = require('haylen.localization')
local ui = require('haylen.ui')

local LanguageTest = require('categories.localization.language-test')
local language = require('categories.localization.language')

local Plurals = haylen.class('Plurals', LanguageTest)

Plurals.hint = 'Change the count with the stepper, or pick another language to see its plural rules in the table.'
Plurals.focus = 'count'
Plurals.keys = {'apples', 'messages', 'lives'}
Plurals.counts = {0, 1, 2, 5, 21}

-- Names the text the count picks from the forms a language file gives the key.
local function form(forms, count)
    if count == 0 and forms.zero then
        return 'zero'
    end
    if count == 1 and forms.one then
        return 'one'
    end
    return 'other'
end

function Plurals:init(entry)
    Plurals.super.init(self, entry)
    self.count = 1
end

function Plurals:content()
    local live = {}
    for _, key in ipairs(Plurals.keys) do
        live[#live + 1] = ui.row{gap = 16,
            ui.label{id = key, text = '', font = 'heading', grow = 1},
            ui.badge{id = key .. '-form', text = '', tone = 'accent'},
        }
    end
    local rows = {}
    for index, count in ipairs(Plurals.counts) do
        local cells = {count}
        for _, key in ipairs(Plurals.keys) do
            cells[#cells + 1] = {key = 'items.' .. key, args = {count = count}}
        end
        rows[index] = {id = 'count-' .. count, cells = cells}
    end
    return {
        ui.panel{width = 720, align = 'stretch', gap = 16,
            ui.formField{label = 'Argument "count"', ui.stepper{id = 'count', value = self.count, min = 0, max = 25, onChange = function(event)
                self.count = event.value
                self:languageChanged()
            end}},
            ui.column{gap = 12, children = live},
            ui.label{id = 'rules', text = '', color = 'textMuted'},
        },
        ui.panel{grow = 1, align = 'stretch', gap = 12,
            ui.sectionTitle{text = 'Every form at a glance'},
            ui.table{id = 'table', columns = {{text = 'Count', width = 120, align = 'end'}, {text = 'Key "items.apples"'}, {text = 'Key "items.messages"'}, {text = 'Key "items.lives"'}}, rows = rows},
        },
    }
end

-- The texts are translations that follow the language on their own, while the badges name the form the language file gives each count.
function Plurals:languageChanged()
    local file = language.file(localization.language())
    local forms = {}
    for _, key in ipairs(Plurals.keys) do
        self:set(key, {text = {key = 'items.' .. key, args = {count = self.count}}})
        self:set(key .. '-form', {text = 'Form "' .. form(file.items[key], self.count) .. '"'})
        local names = {}
        for _, name in ipairs({'zero', 'one', 'other'}) do
            if file.items[key][name] then
                names[#names + 1] = name
            end
        end
        forms[#forms + 1] = string.format('The key "items.%s" has "%s".', key, table.concat(names, '", "'))
    end
    self:set('rules', {text = string.format('The forms of "%s":\n%s', localization.language(), table.concat(forms, '\n'))})
end

return Plurals
