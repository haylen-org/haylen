-- The groups of a language file become dotted keys, so `story.chapters.first.title` reads the title inside `story`, `chapters` and `first`. Groups are not keys themselves, and a table of plural category texts is a plural form rather than a group. The test opens on a key, and shows the key in every language file next to what the current language reads.
local haylen = require('haylen')
local localization = require('haylen.localization')
local ui = require('haylen.ui')

local LanguageTest = require('categories.localization.language-test')
local language = require('categories.localization.language')

local Nested = haylen.class('Nested', LanguageTest)

Nested.code = [[
localization.text('story.chapters.first.title')  -- Groups read with dotted keys.
localization.text('items.apples', {count = 3})
localization.has('story.chapters')  -- A group is not a key.
ui.tree{id = 'tree', items = items, expanded = {'story', 'story.chapters'}}]]

Nested.hint = 'Open the groups of the tree and pick a key or a group to read it with "localization.text" and "localization.has".'
Nested.focus = 'tree'
Nested.first = 'story.chapters.first.title'
-- The tree names keys, so their letters keep their own glyphs and `first` never draws the ligature of f and i.
Nested.keyStyle = {fonts = {body = {ligatures = false}}}
Nested.pluralParts = {zero = true, one = true, two = true, few = true, many = true, other = true}

local function isPlural(value)
    for part in pairs(value) do
        if not Nested.pluralParts[part] then
            return false
        end
    end
    return value.other ~= nil
end

local function count(group)
    local keys = 0
    for _, value in pairs(group) do
        keys = keys + ((type(value) == 'table' and not isPlural(value)) and count(value) or 1)
    end
    return keys
end

-- Builds tree items from a language table, sorted by name with the story, which nests deepest, first, with the translation of every key as its caption and the size of every group.
local function items(group, prefix)
    local names = {}
    for name in pairs(group) do
        names[#names + 1] = name
    end
    table.sort(names, function(a, b)
        if (a == 'story') ~= (b == 'story') then
            return a == 'story'
        end
        return a < b
    end)
    local list = {}
    for index, name in ipairs(names) do
        local key = prefix == '' and name or prefix .. '.' .. name
        local value = group[name]
        if type(value) == 'table' and not isPlural(value) then
            list[index] = {id = key, text = name, caption = string.format('A group of %d keys', count(value)), children = items(value, key)}
        else
            list[index] = {id = key, text = name, caption = {key = key, args = {count = 3}}}
        end
    end
    return list
end

-- The value of a dotted key in the file of a language: a text, the forms of a plural, a group or `nil`.
local function lookup(tag, key)
    local value = language.file(tag)
    for part in key:gmatch('[^.]+') do
        value = type(value) == 'table' and value[part] or nil
    end
    return value
end

-- Reads a dotted key from the file of a language, with a plural form read for three.
local function read(tag, key)
    local value = lookup(tag, key)
    if type(value) == 'table' and isPlural(value) then
        return (value.few or value.other):gsub('{count}', '3')
    end
    return type(value) == 'string' and value or nil
end

function Nested:content()
    local rows = {}
    for index, tag in ipairs(language.tags) do
        rows[index] = ui.row{gap = 16, alignItems = 'start',
            ui.badge{text = tag, tone = 'accent', width = 96},
            ui.label{id = 'in-' .. tag, text = '', grow = 1},
        }
    end
    return {
        ui.panel{width = 760, align = 'stretch', gap = 12,
            ui.sectionTitle{text = 'The groups of "' .. language.folder .. '/en.json"'},
            ui.scroll{grow = 1, height = 0, ui.tree{id = 'tree', items = items(language.file('en'), ''), expanded = {'story', 'story.chapters', 'story.chapters.first'}, selected = Nested.first, style = Nested.keyStyle, onSelect = function(event)
                self:pick(event.item)
            end}},
        },
        ui.panel{grow = 1, align = 'stretch', gap = 16,
            ui.label{id = 'call', text = '', font = 'monospace', color = 'accentText'},
            ui.label{id = 'result', text = '', font = 'heading'},
            ui.row{gap = 12, ui.badge{id = 'has', text = '', tone = 'success'}, ui.label{id = 'hasText', text = '', color = 'textMuted', grow = 1}},
            ui.divider{},
            ui.label{text = 'The same key in every language file', font = 'caption', color = 'textMuted'},
            ui.scroll{grow = 1, height = 0, ui.column{gap = 12, children = rows}},
        },
    }
end

function Nested:enter()
    Nested.super.enter(self)
    self:pick(Nested.first)
end

-- Plural forms read with a count of three, which the call shows, and groups show that they are not keys.
function Nested:pick(key)
    self.key = key
    local has = localization.has(key)
    local value = lookup('en', key)
    local plural = type(value) == 'table' and isPlural(value)
    self:set('call', {text = string.format(plural and "localization.text('%s', {count = 3})" or "localization.text('%s')", key)})
    self:set('result', {text = has and {key = key, args = {count = 3}} or key})
    self:set('has', {text = has and 'A key' or 'A group', tone = has and 'success' or 'information'})
    self:set('hasText', {text = string.format('The call "localization.has" returns "%s"%s.', tostring(has), has and '' or ', and "localization.text" returns the group unchanged, since a group is not a key')})
    for _, tag in ipairs(language.tags) do
        local text = read(tag, key)
        self:set('in-' .. tag, {text = text or (has and 'This file lacks it, so the fallback language fills it in.' or 'A group in this file too.'), color = text and 'text' or 'textMuted', language = tag})
    end
end

function Nested:languageChanged()
    if self.key then
        self:pick(self.key)
    end
end

return Nested
