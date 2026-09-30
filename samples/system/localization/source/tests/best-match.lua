-- Best match: system.info of haylen.system reports the language of the device as a tag such as pt-BR, and localization.findBestMatch finds the language of the app closest to it: the exact tag, or the same base language, ignoring case and treating _ like -.
local haylen = require('haylen')
local localization = require('haylen.localization')
local system = require('haylen.system')
local ui = require('haylen.ui')

local language = require('language')
local sample = require('sample')

local BestMatch = haylen.class('BestMatch', sample.Test)

BestMatch.hints = 'Use the language of the device, type any tag and press Enter, or pick a row of the table to switch to its match.'

local kTags = {'pt-BR', 'pt-PT', 'pt', 'PT_br', 'es-MX', 'es-419', 'en-GB', 'EN-us', 'ja-JP', 'fr-FR', 'zh-Hans-CN'}

-- Tells how a tag matched: the same tag, the same base language or nothing.
local function reason(tag, match)
    if match == nil then
        return 'No language of the app'
    end
    if tag:lower():gsub('_', '-') == match:lower() then
        return 'The same tag'
    end
    return 'The same base language'
end

function BestMatch:init(entry)
    BestMatch.super.init(self, entry)
    self.typed = 'es-AR'
end

function BestMatch:content()
    local rows = {}
    for index, tag in ipairs(kTags) do
        local match = localization.findBestMatch(tag)
        rows[index] = {id = tag, cells = {tag, match or 'none', reason(tag, match)}}
    end
    return {
        ui.panel{width = 640, align = 'stretch', gap = 12,
            ui.sectionTitle{text = 'This device'},
            ui.label{id = 'locale', text = ''},
            ui.button{id = 'device', text = 'Use the language of the device', enabled = false, onClick = function()
                language.use(self.deviceMatch)
            end},
            ui.sectionTitle{text = 'Any tag'},
            ui.textField{id = 'tag', value = self.typed, maxLength = 35, autocorrect = false, autocapitalize = 'none', returnKey = 'done', onChange = function(event)
                self:typeTag(event.value)
            end, onSubmit = function(event)
                local match = localization.findBestMatch(event.value)
                if match then
                    language.use(match)
                end
            end},
            ui.label{id = 'typed', text = '', font = 'monospace'},
        },
        ui.panel{grow = 1, align = 'stretch', gap = 12,
            ui.label{text = 'The app has ' .. table.concat(localization.languages(), ', ') .. '.', color = 'textMuted'},
            ui.table{id = 'tags', columns = {{text = 'Tag', width = 260}, {text = 'localization.findBestMatch', width = 360}, {text = 'Why'}}, rows = rows, onSelect = function(event)
                local match = localization.findBestMatch(event.item)
                if match then
                    language.use(match)
                end
            end},
        },
    }
end

-- A platform that does not report the language of the device leaves the locale of system.info nil.
function BestMatch:enter()
    BestMatch.super.enter(self)
    self:typeTag(self.typed)
    local tag = system.info().locale
    if not tag then
        self:show('locale', {text = 'The platform does not report the language of the device.', color = 'dangerText'})
        return
    end
    self.deviceMatch = localization.findBestMatch(tag)
    self:show('locale', {text = string.format('The device language is %s, and findBestMatch picks %s.', tag, self.deviceMatch or 'nothing')})
    self:show('device', {enabled = self.deviceMatch ~= nil})
end

function BestMatch:typeTag(tag)
    self.typed = tag
    local match = tag ~= '' and localization.findBestMatch(tag) or nil
    self:show('typed', {text = string.format("localization.findBestMatch('%s')\nreturns %s\n%s", tag, match and "'" .. match .. "'" or 'nil', reason(tag, match))})
end

return BestMatch
