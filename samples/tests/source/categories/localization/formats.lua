-- Numbers, dates and lists in many languages: decimals, percentages, amounts and compact numbers with the marks of each language, a moment in every date style with the time, a list joined with "and" and "or", and the plural and ordinal categories of the counts from 0 to 25.
local haylen = require('haylen')
local localization = require('haylen.localization')
local ui = require('haylen.ui')

local Test = require('harness.test')

local Formats = haylen.class('Formats', Test)

Formats.code = [[
localization.formatNumber(1234.5, {style = 'currency', currency = 'EUR', language = 'de'})
localization.formatNumber(1530000, {style = 'compact'})
localization.formatDate(os.time(), {date = 'long', time = true})
localization.formatList({'Ada', 'Grace', 'Hedy'}, {type = 'or'})
localization.pluralCategory(21, {language = 'ru'})  localization.pluralCategory(2, {ordinal = true})]]

local kLanguages = {
    {id = 'en', text = 'English'}, {id = 'en-GB', text = 'British English'}, {id = 'de', text = 'German'}, {id = 'fr', text = 'French'}, {id = 'pt-BR', text = 'Brazilian Portuguese'},
    {id = 'ru', text = 'Russian'}, {id = 'pl', text = 'Polish'}, {id = 'ar', text = 'Arabic'}, {id = 'ja', text = 'Japanese'}, {id = 'hi', text = 'Hindi'},
}
local kMoment = 1791900300

function Formats:enter()
    self.language = 'en'
    self:frame{
        hint = 'Pick a language to see how it writes numbers, dates, lists and plural categories.',
        code = Formats.code,
        content = {ui.row{grow = 1, gap = 24,
            ui.panel{width = 420, align = 'stretch', gap = 12,
                ui.formField{label = 'Language', ui.radioGroup{id = 'language', items = kLanguages, selected = self.language, onChange = function(event)
                    self.language = event.value
                    self:show()
                end}},
            },
            ui.panel{grow = 1, align = 'stretch', gap = 12,
                ui.label{id = 'numbers', text = '', font = 'monospace'},
                ui.label{id = 'dates', text = '', font = 'monospace'},
                ui.label{id = 'plurals', text = '', font = 'monospace'},
            },
        }},
    }
    self:show()
end

function Formats:show()
    local language = self.language
    local function number(value, options)
        options = options or {}
        options.language = language
        return localization.formatNumber(value, options)
    end
    self:set('numbers', {text = table.concat({
        'Decimal ' .. number(1234567.891),
        'Percent ' .. number(0.256, {style = 'percent'}),
        'Currency ' .. number(1234.5, {style = 'currency', currency = 'EUR'}),
        'Compact ' .. number(1530000, {style = 'compact'}) .. ', ' .. number(12345, {style = 'compact'}),
        'List ' .. localization.formatList({'Ada', 'Grace', 'Hedy'}, {language = language}) .. ' / ' .. localization.formatList({'Ada', 'Grace'}, {type = 'or', language = language}),
    }, '\n')})

    local dates = {}
    for _, style in ipairs({'short', 'medium', 'long', 'full'}) do
        dates[#dates + 1] = style:sub(1, 1):upper() .. style:sub(2) .. ' ' .. localization.formatDate(kMoment, {date = style, time = style == 'full', timeZoneOffset = 0, language = language})
    end
    self:set('dates', {text = table.concat(dates, '\n')})

    local plurals = {}
    for count = 0, 25 do
        plurals[#plurals + 1] = string.format('%d %s %s', count, localization.pluralCategory(count, {language = language}), localization.pluralCategory(count, {ordinal = true, language = language}))
    end
    self:set('plurals', {text = 'Count, plural and ordinal categories\n' .. table.concat(plurals, ', ')})
end

return Formats
