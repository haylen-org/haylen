-- The function `localization.findBestMatch` finds the language of the app closest to a tag: the tag itself, then the tag without its last subtags, such as `pt` for `pt-BR`, then the first language of the app with the same language and a script that agrees, such as `pt-BR` for `pt-PT`. Tags may use `-` or `_` in any case, and `localization.normalizeTag` writes them the canonical way. The function `system.languages` lists the languages the person prefers on the device.
local haylen = require('haylen')
local localization = require('haylen.localization')
local system = require('haylen.system')
local ui = require('haylen.ui')

local LanguageTest = require('categories.localization.language-test')

local BestMatch = haylen.class('BestMatch', LanguageTest)

BestMatch.code = [[
localization.normalizeTag('PT_br')  -- "pt-BR"
localization.findBestMatch('pt-PT')  -- The tag, a shorter tag, another region or nil.
for _, tag in ipairs(system.languages()) do
    print(tag, localization.findBestMatch(tag))
end]]

BestMatch.hint = 'Use the first language of the device, type any tag and press Enter, or pick a row of the table to switch to its match.'
BestMatch.tags = {'pt-BR', 'pt-PT', 'pt', 'PT_br', 'es-MX', 'es-419', 'en-GB', 'EN-us', 'ja-JP', 'fr-FR', 'zh-Hans-CN', 'en-US-u-mu-celsius'}

-- Tells how a tag matched: the same tag, a shorter tag, the language with a region, another region of the same language or nothing.
function BestMatch.reason(tag, match)
    if match == nil then
        return 'No language of the app'
    end
    local canonical = localization.normalizeTag(tag)
    if canonical == match then
        return 'The same tag'
    end
    if canonical:sub(1, #match + 1) == match .. '-' then
        return 'A shorter tag'
    end
    if not canonical:find('-', 1, true) then
        return 'The language with a region'
    end
    return 'Another region of the language'
end

function BestMatch:init(entry)
    BestMatch.super.init(self, entry)
    self.typed = 'es-AR'
end

function BestMatch:content()
    local rows = {}
    for index, tag in ipairs(BestMatch.tags) do
        local match = localization.findBestMatch(tag)
        rows[index] = {id = tag, cells = {tag, localization.normalizeTag(tag), match or 'None', BestMatch.reason(tag, match)}}
    end
    return {
        ui.panel{width = 640, align = 'stretch',
            ui.scroll{grow = 1, height = 0, ui.column{gap = 12,
                ui.sectionTitle{text = 'This device'},
                ui.label{id = 'locale', text = ''},
                ui.button{id = 'device', text = 'Use the first language of the device', enabled = false, align = 'stretch', onClick = function()
                    localization.setLanguage(self.deviceMatch)
                end},
                ui.sectionTitle{text = 'Any tag'},
                ui.textField{id = 'tag', value = self.typed, maxLength = 35, autocorrect = false, autocapitalize = 'none', returnKey = 'done', onChange = function(event)
                    self:typeTag(event.value)
                end, onSubmit = function(event)
                    local valid, match = pcall(localization.findBestMatch, event.value)
                    if valid and match then
                        localization.setLanguage(match)
                    end
                end},
                ui.label{id = 'typed', text = '', color = 'textMuted'},
            }},
        },
        ui.panel{grow = 1, align = 'stretch', gap = 12,
            ui.label{text = 'The app has "' .. table.concat(localization.languages(), '", "') .. '".', color = 'textMuted'},
            ui.scroll{grow = 1, height = 0, ui.table{id = 'tags', columns = {{text = 'Tag', width = 260}, {text = 'Canonical', width = 200}, {text = 'Best match', width = 160}, {text = 'Why'}}, rows = rows, onSelect = function(event)
                local match = localization.findBestMatch(event.item)
                if match then
                    localization.setLanguage(match)
                end
            end}},
        },
    }
end

-- A platform that tells no language of the person leaves the list of `system.languages` empty.
function BestMatch:enter()
    BestMatch.super.enter(self)
    self:typeTag(self.typed)
    local tag = system.languages()[1]
    if not tag then
        self:set('locale', {text = 'The platform does not report the languages of the device.', color = 'dangerText'})
        return
    end
    self.deviceMatch = localization.findBestMatch(tag)
    self:set('locale', {text = string.format('The device prefers "%s", and "findBestMatch" picks %s.', table.concat(system.languages(), '", "'), self.deviceMatch and '"' .. self.deviceMatch .. '"' or 'nothing')})
    self:set('device', {enabled = self.deviceMatch ~= nil})
end

-- A typed text that is no language tag shows the sentence that `normalizeTag` raises.
function BestMatch:typeTag(tag)
    self.typed = tag
    local valid, result = pcall(localization.normalizeTag, tag)
    if not valid then
        self:set('typed', {text = tostring(result):gsub('^.-:%d+: ', '')})
        return
    end
    local match = localization.findBestMatch(tag)
    self:set('typed', {text = string.format('The tag "%s" is "%s", and "findBestMatch" returns %s: %s.', tag, result, match and '"' .. match .. '"' or '"nil"', BestMatch.reason(tag, match):lower())})
end

return BestMatch
