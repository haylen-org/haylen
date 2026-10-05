-- Preferences are one table of nested groups addressed by dotted keys, changed in memory by `set` and `remove`, written to `preferences.json` by `save` and read back by `load`, which also throws away unsaved changes.
local haylen = require('haylen')
local json = require('json')
local preferences = require('haylen.preferences')
local storage = require('haylen.storage')
local ui = require('haylen.ui')

local Test = require('harness.test')
local settings = require('categories.preferences.settings')

local Values = haylen.class('Values', Test)

-- Examples to try, each with a key, a value as typed and how to read it.
Values.examples = {
    {id = 'name', key = 'profile.name', value = 'Ana', kind = 'text'},
    {id = 'coins', key = 'profile.stats.coins', value = '120', kind = 'number'},
    {id = 'tutorial', key = 'profile.tutorialDone', value = 'true', kind = 'boolean'},
    {id = 'recent', key = 'recent', value = 'slot-1, slot-3', kind = 'list'},
    {id = 'group', key = 'profile.stats', value = '', kind = 'text'},
    {id = 'through', key = 'profile.name.first', value = 'Ana', kind = 'text'},
    {id = 'empty', key = 'profile..name', value = 'Ana', kind = 'text'},
    {id = 'reserved', key = 'audio.volume.music', value = '0.5', kind = 'number'},
}

-- Formats a Lua value as indented JSON, the way `preferences.json` holds it.
local function show(value)
    if value == nil then
        return 'nil'
    end
    return json.encode(value, {pretty = true})
end

-- Turns the typed text into the value of its kind, or returns `nil` and why it cannot.
local function parse(text, kind)
    if kind == 'number' then
        local number = tonumber(text)
        return number, number == nil and 'The value "' .. text .. '" is not a number.' or nil
    end
    if kind == 'boolean' then
        if text ~= 'true' and text ~= 'false' then
            return nil, 'A boolean is "true" or "false".'
        end
        return text == 'true'
    end
    if kind == 'list' then
        local list = {}
        for part in text:gmatch('[^,]+') do
            list[#list + 1] = part:match('^%s*(.-)%s*$')
        end
        return list
    end
    return text
end

-- Tells why the playground leaves a group alone: the other tests of the category keep their settings in some groups, and the project keeps the last test opened in `tests`.
local function reserved(group)
    if group == 'tests' then
        return 'The group "tests" keeps the last test the project opened, so this playground leaves it alone.'
    end
    for _, name in ipairs(settings.groups) do
        if name == group then
            return 'The group "' .. group .. '" belongs to the other tests of the category, so this playground leaves it alone.'
        end
    end
    return nil
end

function Values:init(entry)
    Values.super.init(self, entry)
    self.key = Values.examples[1].key
    self.value = Values.examples[1].value
    self.kind = Values.examples[1].kind
end

function Values:enter()
    local examples = {}
    for index, example in ipairs(Values.examples) do
        examples[index] = {id = example.id, text = example.key, caption = example.value ~= '' and example.kind:sub(1, 1):upper() .. example.kind:sub(2) .. ' ' .. example.value or 'A group'}
    end
    local actions = {
        {id = 'set', text = 'Set', run = self.setValue},
        {id = 'get', text = 'Get', run = self.getValue},
        {id = 'has', text = 'Has', run = self.hasValue},
        {id = 'remove', text = 'Remove', run = self.removeValue},
        {id = 'save', text = 'Save', run = self.save},
        {id = 'load', text = 'Load', run = self.reload},
    }
    local buttons = {}
    for index, action in ipairs(actions) do
        buttons[index] = ui.button{id = action.id, text = action.text, align = 'stretch', onClick = function()
            action.run(self)
            self:refresh()
        end}
    end
    self:frame{
        hint = 'Type a dotted key and a value, or pick an example, then set, read, remove, save or load. The middle column is memory and the right column is the file.',
        focus = 'examples',
        content = {ui.row{grow = 1, gap = 24,
            ui.panel{width = 640, align = 'stretch', gap = 12,
                ui.formField{label = 'Key', ui.textField{id = 'key', value = self.key, autocorrect = false, autocapitalize = 'none', returnKey = 'next', onChange = function(event)
                    self.key = event.value
                end}},
                ui.formField{label = 'Value', ui.textField{id = 'value', value = self.value, autocorrect = false, autocapitalize = 'none', returnKey = 'done', onChange = function(event)
                    self.value = event.value
                end}},
                ui.segmentedControl{id = 'kind', selected = self.kind, items = {{id = 'text', text = 'Text'}, {id = 'number', text = 'Number'}, {id = 'boolean', text = 'Boolean'}, {id = 'list', text = 'List'}}, onChange = function(event)
                    self.kind = event.value
                end},
                ui.grid{columns = 3, gap = 12, children = buttons},
                ui.label{id = 'result', text = 'Pick an example or type a key.', color = 'accentText'},
                ui.sectionTitle{text = 'Examples'},
                ui.scroll{grow = 1, ui.list{id = 'examples', items = examples, onSelect = function(event)
                    self:pick(event.item)
                end}},
            },
            ui.panel{grow = 1, align = 'stretch', gap = 12,
                ui.row{gap = 12, ui.sectionTitle{text = 'In memory', grow = 1}, ui.badge{id = 'dirty', text = 'Saved', tone = 'success'}},
                ui.label{text = 'The table of "preferences.values()"', font = 'caption', color = 'textMuted'},
                ui.scroll{grow = 1, ui.label{id = 'memory', text = '', font = 'monospace'}},
            },
            ui.panel{grow = 1, align = 'stretch', gap = 12,
                ui.sectionTitle{text = 'On disk'},
                ui.label{text = 'The file "preferences.json" of the user folder', font = 'caption', color = 'textMuted'},
                ui.scroll{grow = 1, ui.label{id = 'disk', text = '', font = 'monospace'}},
            },
        }},
    }
    self:refresh()
end

function Values:pick(id)
    for _, example in ipairs(Values.examples) do
        if example.id == id then
            self.key, self.value, self.kind = example.key, example.value, example.kind
            self:set('key', {value = example.key})
            self:set('value', {value = example.value})
            self:set('kind', {selected = example.kind})
        end
    end
end

-- Runs a call that may raise, such as a key with an empty part, and shows its result or its error.
function Values:report(call, ...)
    local ok, result = pcall(...)
    self:set('result', {text = ok and call .. ' ' .. result or call .. ' raised: ' .. tostring(result), color = ok and 'accentText' or 'dangerText'})
end

function Values:setValue()
    local value, problem = parse(self.value, self.kind)
    local group = self.key:match('^[^.]*')
    if reserved(group) then
        problem = reserved(group)
    elseif self.value == '' then
        problem = 'Type a value first. A key that names a group, such as "profile.stats", is read with Get.'
    end
    if problem then
        self:set('result', {text = problem, color = 'dangerText'})
        return
    end
    self:report(string.format('The call "preferences.set(\'%s\', value)"', self.key), function()
        preferences.set(self.key, value)
        return 'stored it in memory.'
    end)
end

function Values:getValue()
    self:report(string.format('The call "preferences.get(\'%s\')" returned', self.key), function()
        return show(preferences.get(self.key))
    end)
end

function Values:hasValue()
    self:report(string.format('The call "preferences.has(\'%s\')" returned', self.key), function()
        return '"' .. tostring(preferences.has(self.key)) .. '".'
    end)
end

function Values:removeValue()
    local group = self.key:match('^[^.]*')
    if reserved(group) then
        self:set('result', {text = reserved(group), color = 'dangerText'})
        return
    end
    self:report(string.format('The call "preferences.remove(\'%s\')" returned', self.key), function()
        return '"' .. tostring(preferences.remove(self.key)) .. '".'
    end)
end

function Values:save()
    self:report('The call "preferences.save()"', function()
        preferences.save()
        return 'wrote "preferences.json".'
    end)
end

function Values:reload()
    self:report('The call "preferences.load()"', function()
        preferences.load()
        return 'read "preferences.json" and dropped the unsaved changes.'
    end)
end

-- Shows the preferences in memory next to the file on disk, with a badge that tells whether they differ.
function Values:refresh()
    local dirty = preferences.dirty()
    self:set('dirty', {text = dirty and 'Unsaved changes' or 'Saved', tone = dirty and 'warning' or 'success'})
    self:set('memory', {text = show(preferences.values())})
    self:set('disk', {text = storage.exists('preferences.json') and storage.readText('preferences.json') or 'There is no "preferences.json" yet.'})
end

return Values
