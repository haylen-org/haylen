-- Numbers, dates and lists in many languages: decimals, percentages, amounts and compact numbers with the marks of each language, a moment in every date style with the time, a list joined with "and" and "or", and the plural and ordinal categories of the counts from 0 to 25, colored by category.
local haylen = require('haylen')
local localization = require('haylen.localization')
local ui = require('haylen.ui')

local Test = require('harness.test')

local Formats = haylen.class('Formats', Test)

Formats.code = [[
localization.formatNumber(1234.5, {style = 'currency', currency = 'EUR', language = language})
localization.formatNumber(1530000, {style = 'compact', language = language})
localization.formatDate(os.time(), {date = 'long', time = true, language = language})
localization.formatList({'Ada', 'Grace', 'Hedy'}, {type = 'or', language = language})
localization.pluralCategory(21, {language = language})
localization.pluralCategory(2, {ordinal = true, language = language})]]

Formats.languages = {
    {id = 'en', text = 'English'}, {id = 'en-GB', text = 'British English'}, {id = 'de', text = 'German'}, {id = 'fr', text = 'French'}, {id = 'pt-BR', text = 'Brazilian Portuguese'},
    {id = 'ru', text = 'Russian'}, {id = 'pl', text = 'Polish'}, {id = 'ar', text = 'Arabic'}, {id = 'ja', text = 'Japanese'}, {id = 'hi', text = 'Hindi'},
}
Formats.moment = 1791900300
Formats.counts = 25
Formats.tones = {zero = 'neutral', one = 'accent', two = 'success', few = 'warning', many = 'danger', other = 'information'}
Formats.categories = {'zero', 'one', 'two', 'few', 'many', 'other'}

local function capitalized(word)
    return word:sub(1, 1):upper() .. word:sub(2)
end

function Formats:enter()
    self.language = 'en'
    local legend = {ui.label{text = 'The colors of the categories', color = 'textMuted', margin = {0, 8, 0, 0}}}
    for _, category in ipairs(Formats.categories) do
        legend[#legend + 1] = ui.badge{text = capitalized(category), tone = Formats.tones[category], solid = true}
    end
    self:frame{
        hint = 'Pick a language to see how it writes numbers, dates, lists and plural categories.',
        code = Formats.code,
        focus = 'language',
        content = {
            ui.row{gap = 16,
                ui.label{text = 'Language', font = 'heading'},
                ui.combo{id = 'language', width = 480, items = Formats.languages, selected = self.language, onChange = function(event)
                    self.language = event.value
                    self:show()
                end},
                ui.row{grow = 1, gap = 8, justify = 'end', children = legend},
            },
            ui.panel{grow = 1, align = 'stretch',
                ui.scroll{grow = 1, height = 0, ui.column{gap = 20, style = {metrics = {listRowHeight = 48}},
                    ui.row{gap = 24, alignItems = 'start',
                        ui.column{grow = 2, gap = 8,
                            ui.sectionTitle{text = 'Numbers and lists'},
                            ui.table{id = 'numbers', columns = {{text = 'Style', width = 150}, {text = 'Result'}}, rows = {}},
                        },
                        ui.column{grow = 3, gap = 8,
                            ui.sectionTitle{text = 'Dates'},
                            ui.table{id = 'dates', columns = {{text = 'Style', width = 150}, {text = 'Result'}}, rows = {}},
                        },
                    },
                    ui.row{gap = 24, alignItems = 'start',
                        ui.column{grow = 1, gap = 10,
                            ui.sectionTitle{text = 'Plural categories of 0 to 25'},
                            ui.grid{id = 'plurals', columns = 13, gap = 6},
                        },
                        ui.column{grow = 1, gap = 10,
                            ui.sectionTitle{text = 'Ordinal categories of 0 to 25'},
                            ui.grid{id = 'ordinals', columns = 13, gap = 6},
                        },
                    },
                }},
            },
        },
    }
    self:show()
end

-- The counts as badges in the color of their category.
function Formats:badges(prefix, ordinal)
    local badges = {}
    for count = 0, Formats.counts do
        local category = localization.pluralCategory(count, {ordinal = ordinal, language = self.language})
        badges[#badges + 1] = ui.badge{id = prefix .. count, text = count, tone = Formats.tones[category], solid = true, tooltip = capitalized(category), align = 'stretch'}
    end
    return badges
end

function Formats:show()
    local language = self.language
    local function number(value, options)
        options = options or {}
        options.language = language
        return localization.formatNumber(value, options)
    end
    self:set('numbers', {rows = {
        {id = 'decimal', cells = {'Decimal', number(1234567.891)}},
        {id = 'percent', cells = {'Percent', number(0.256, {style = 'percent'})}},
        {id = 'currency', cells = {'Currency', number(1234.5, {style = 'currency', currency = 'EUR'})}},
        {id = 'compact', cells = {'Compact', number(1530000, {style = 'compact'}) .. ', ' .. number(12345, {style = 'compact'})}},
        {id = 'and', cells = {'List', localization.formatList({'Ada', 'Grace', 'Hedy'}, {language = language})}},
        {id = 'or', cells = {'Choice', localization.formatList({'Ada', 'Grace'}, {type = 'or', language = language})}},
    }})

    local dates = {}
    for _, style in ipairs({'short', 'medium', 'long', 'full'}) do
        dates[#dates + 1] = {id = style, cells = {capitalized(style), localization.formatDate(Formats.moment, {date = style, time = style == 'full', timeZoneOffset = 0, language = language})}}
    end
    self:set('dates', {rows = dates})
    self.gui:replaceChildren('plurals', self:badges('plural-', false))
    self.gui:replaceChildren('ordinals', self:badges('ordinal-', true))
end

return Formats
