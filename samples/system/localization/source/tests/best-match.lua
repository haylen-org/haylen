-- Best match: the system.locale call of haylen.platform returns the language of the device as a tag such as pt-BR, and localization.bestMatch finds the language of the app closest to it: the exact tag, or the same base language, ignoring case and treating _ like -.
local haylen = require('haylen')
local localization = require('haylen.localization')
local platform = require('haylen.platform')
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
        local match = localization.bestMatch(tag)
        rows[index] = {id = tag, cells = {tag, match or 'none', reason(tag, match)}}
    end
    return {
        ui.panel{width = 640, align = 'stretch', gap = 12,
            ui.sectionTitle{text = 'This device'},
            ui.label{id = 'locale', text = "Asking platform.call('system.locale')…"},
            ui.button{id = 'device', text = 'Use the language of the device', enabled = false, onClick = function()
                language.use(self.deviceMatch)
            end},
            ui.sectionTitle{text = 'Any tag'},
            ui.textField{id = 'tag', value = self.typed, maxLength = 35, autocorrect = false, autocapitalize = 'none', returnKey = 'done', onChange = function(event)
                self:typeTag(event.value)
            end, onSubmit = function(event)
                local match = localization.bestMatch(event.value)
                if match then
                    language.use(match)
                end
            end},
            ui.label{id = 'typed', text = '', font = 'monospace'},
        },
        ui.panel{grow = 1, align = 'stretch', gap = 12,
            ui.label{text = 'The app has ' .. table.concat(localization.languages(), ', ') .. '.', color = 'textMuted'},
            ui.table{id = 'tags', columns = {{text = 'Tag', width = 260}, {text = 'localization.bestMatch', width = 360}, {text = 'Why'}}, rows = rows, onSelect = function(event)
                local match = localization.bestMatch(event.item)
                if match then
                    language.use(match)
                end
            end},
        },
    }
end

-- The platform answers on a later frame, in a task of the scene, which stops if the player leaves first.
function BestMatch:enter()
    BestMatch.super.enter(self)
    self:typeTag(self.typed)
    self:spawn(function()
        local tag, failure = platform.call('system.locale'):await()
        if not tag then
            self:show('locale', {text = 'The platform did not answer: ' .. tostring(failure), color = 'dangerText'})
            return
        end
        self.deviceMatch = localization.bestMatch(tag)
        self:show('locale', {text = string.format('The device language is %s, and bestMatch picks %s.', tag, self.deviceMatch or 'nothing')})
        self:show('device', {enabled = self.deviceMatch ~= nil})
    end)
end

function BestMatch:typeTag(tag)
    self.typed = tag
    local match = tag ~= '' and localization.bestMatch(tag) or nil
    self:show('typed', {text = string.format("localization.bestMatch('%s')\nreturns %s\n%s", tag, match and "'" .. match .. "'" or 'nil', reason(tag, match))})
end

return BestMatch
