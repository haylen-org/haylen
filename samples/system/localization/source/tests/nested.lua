-- Nested keys: the groups of a language file become dotted keys, so `story.chapters.first.title` reads the title inside `story`, `chapters` and `first`. Groups are not keys themselves, and a table of `zero`, `one` and `other` texts is a plural form rather than a group.
local assets = require('haylen.assets')
local haylen = require('haylen')
local localization = require('haylen.localization')
local ui = require('haylen.ui')

local sample = require('sample')

local Nested = haylen.class('Nested', sample.Test)

Nested.hints = 'Open the groups of the tree and pick a key or a group to read it with "localization.text" and "localization.has".'
Nested.focus = 'tree'

local kPluralParts = {zero = true, one = true, other = true}

local function isPlural(value)
    for part in pairs(value) do
        if not kPluralParts[part] then
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
            list[index] = {id = key, text = name, children = items(value, key)}
        else
            list[index] = {id = key, text = name, caption = {key = key, args = {count = 3}}}
        end
    end
    return list
end

function Nested:content()
    return {
        ui.panel{width = 820, align = 'stretch', gap = 12,
            ui.sectionTitle{text = 'The groups of "locale/en.json"'},
            ui.scroll{grow = 1, ui.tree{id = 'tree', items = items(assets.json('locale/en.json'), ''), expanded = {'story', 'story.chapters', 'story.chapters.first'}, onSelect = function(event)
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
    self:show('call', {text = string.format("localization.text('%s', {count = 3})", key)})
    self:show('result', {text = has and {key = key, args = {count = 3}} or key})
    self:show('has', {text = string.format("localization.has('%s') is %s%s", key, tostring(has), has and '' or ', since a group is not a key, and "text" returns the key unchanged')})
end

function Nested:languageChanged()
    if self.key then
        self:pick(self.key)
    end
end

return Nested
