-- A key whose table holds texts of plural categories is a plural form, and the `count` argument picks the text by the plural rules of the language: `zero` for 0 when the form has it, then the category of the count when the form has it, and `other` otherwise. Arabic counts in six categories, so its forms have `two`, `few` and `many` texts too, and Japanese counts without a singular, so its forms have no `one` text.
local haylen = require('haylen')
local localization = require('haylen.localization')
local ui = require('haylen.ui')

local LanguageTest = require('categories.localization.language-test')
local language = require('categories.localization.language')

local Plurals = haylen.class('Plurals', LanguageTest)

Plurals.code = [[
localization.text('items.apples', {count = 5})  -- The form picked by the count.
localization.pluralCategory(11, {language = 'ar'})  -- "many", by the rules of Arabic.
ui.table{id = 'table', columns = columns, rows = {
    {id = 'count-2', cells = {2, {key = 'items.apples', args = {count = 2}}}},
}}]]

Plurals.hint = 'Change the count with the stepper, or pick another language, such as Arabic, to see its plural rules pick the forms.'
Plurals.focus = 'count'
Plurals.keys = {'apples', 'messages', 'lives'}
Plurals.counts = {0, 1, 2, 3, 11, 21, 100}
Plurals.categories = {'zero', 'one', 'two', 'few', 'many', 'other'}

-- Names the text the count picks from the forms a language file gives the key.
local function form(forms, count)
    if count == 0 and forms.zero then
        return 'zero'
    end
    local category = localization.pluralCategory(count)
    return forms[category] and category or 'other'
end

function Plurals:init(entry)
    Plurals.super.init(self, entry)
    self.count = 1
end

function Plurals:content()
    local live = {}
    for _, key in ipairs(Plurals.keys) do
        local badges = {}
        for _, category in ipairs(Plurals.categories) do
            badges[#badges + 1] = ui.badge{id = key .. '-' .. category, text = category:sub(1, 1):upper() .. category:sub(2), tone = 'neutral'}
        end
        live[#live + 1] = ui.card{align = 'stretch', gap = 10, padding = {14, 20},
            ui.label{id = key, text = '', font = 'heading'},
            ui.row{gap = 8, children = badges},
        }
    end
    local columns = {{text = 'Count', width = 110, align = 'end'}, {text = 'Category', width = 170}}
    for _, key in ipairs(Plurals.keys) do
        columns[#columns + 1] = {text = 'Key "items.' .. key .. '"'}
    end
    return {
        ui.panel{width = 640, align = 'stretch',
            ui.scroll{grow = 1, height = 0, ui.column{gap = 12,
                ui.formField{label = 'Argument "count"', ui.stepper{id = 'count', value = self.count, min = 0, max = 120, onChange = function(event)
                    self.count = event.value
                    self:languageChanged()
                end}},
                ui.label{id = 'rule', text = '', color = 'textMuted'},
                ui.column{gap = 12, children = live},
            }},
        },
        ui.panel{grow = 1, align = 'stretch', gap = 12,
            ui.sectionTitle{text = 'Every form at a glance'},
            ui.scroll{grow = 1, height = 0, ui.table{id = 'table', columns = columns, rows = self:rows()}},
        },
    }
end

-- The rows of the table, whose categories follow the rules of the current language.
function Plurals:rows()
    local rows = {}
    for index, count in ipairs(Plurals.counts) do
        local cells = {count, localization.pluralCategory(count)}
        for _, key in ipairs(Plurals.keys) do
            cells[#cells + 1] = {key = 'items.' .. key, args = {count = count}}
        end
        rows[index] = {id = 'count-' .. count, cells = cells}
    end
    return rows
end

-- The texts are translations that follow the language on their own, while the badges show the forms the language file gives each key and light the one the count picks.
function Plurals:languageChanged()
    local file = language.file(localization.language())
    for _, key in ipairs(Plurals.keys) do
        local forms = file.items[key]
        local picked = form(forms, self.count)
        self:set(key, {text = {key = 'items.' .. key, args = {count = self.count}}})
        for _, category in ipairs(Plurals.categories) do
            self:set(key .. '-' .. category, {visible = forms[category] ~= nil, tone = category == picked and 'accent' or 'neutral', solid = category == picked})
        end
    end
    self:set('rule', {text = string.format('The rules of "%s" put %d in the category "%s". The badges show the forms of each key, with the one the count picks lit.', localization.language(), self.count, localization.pluralCategory(self.count))})
    self:set('table', {rows = self:rows()})
end

return Plurals
