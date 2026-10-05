-- The groups of a language file become dotted keys, so `story.chapters.first.title` reads the title inside `story`, `chapters` and `first`. Groups are not keys themselves, and a table of `zero`, `one` and `other` texts is a plural form rather than a group.
local haylen = require('haylen')
local localization = require('haylen.localization')
local ui = require('haylen.ui')

local LanguageTest = require('categories.localization.language-test')
local language = require('categories.localization.language')

local Nested = haylen.class('Nested', LanguageTest)

Nested.hint = 'Open the groups of the tree and pick a key or a group to read it with "localization.text" and "localization.has".'
Nested.focus = 'tree'
Nested.pluralParts = {zero = true, one = true, other = true}

local function isPlural(value)
    for part in pairs(value) do
        if not Nested.pluralParts[part] then
            return false
        end
    end
    return value.other ~= nil
end

-- Builds tree items from a language table, sorted by name, with the translation of every key as its caption.
local function items(group, prefix)
    local names = {}
    for name in pairs(group) do
        names[#names + 1] = name
    end
    table.sort(names)
    local list = {}
    for index, name in ipairs(names) do
        local key = prefix == '' and name or prefix .. '.' .. name
        local value = group[name]
        if type(value) == 'table' and not isPlural(value) then
            list[index] = {id = key, text = '"' .. name .. '"', children = items(value, key)}
        else
            list[index] = {id = key, text = '"' .. name .. '"', caption = {key = key, args = {count = 3}}}
        end
    end
    return list
end

function Nested:content()
    return {
        ui.panel{width = 820, align = 'stretch', gap = 12,
            ui.sectionTitle{text = 'The groups of "' .. language.folder .. '/en.json"'},
            ui.scroll{grow = 1, ui.tree{id = 'tree', items = items(language.file('en'), ''), expanded = {'story', 'story.chapters', 'story.chapters.first'}, onSelect = function(event)
                self:pick(event.item)
            end}},
        },
        ui.panel{grow = 1, align = 'stretch', gap = 12,
            ui.label{id = 'call', text = 'Pick a key or a group.', font = 'monospace', color = 'accentText'},
            ui.label{id = 'result', text = '', font = 'heading'},
            ui.label{id = 'has', text = '', color = 'textMuted'},
        },
    }
end

-- Plural forms read with a count of three, and groups show that they are not keys.
function Nested:pick(key)
    self.key = key
    local has = localization.has(key)
    self:set('call', {text = string.format("localization.text('%s', {count = 3})", key)})
    self:set('result', {text = has and {key = key, args = {count = 3}} or '"' .. key .. '"'})
    self:set('has', {text = string.format('The call "localization.has(\'%s\')" returns "%s"%s.', key, tostring(has), has and '' or ', since a group is not a key, and "localization.text" returns the key unchanged')})
end

function Nested:languageChanged()
    if self.key then
        self:pick(self.key)
    end
end

return Nested
