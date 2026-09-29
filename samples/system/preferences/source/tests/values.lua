-- Keys and values: preferences are one table of nested groups addressed by dotted keys, changed in memory by set and remove, written to preferences.json by save and read back by load, which also throws away unsaved changes.
local haylen = require('haylen')
local preferences = require('haylen.preferences')
local storage = require('haylen.storage')
local ui = require('haylen.ui')

local sample = require('sample')

local Values = haylen.class('Values', sample.Test)

Values.hints = 'Type a dotted key and a value, or pick an example, then set, read, remove, save or load. The middle column is memory and the right column is the file.'

-- The groups the other tests keep their settings in, which this playground leaves alone so a typed value never breaks them.
local kReserved = {audio = true, window = true, input = true, game = true, controls = true, settings = true}
Values.focus = 'examples'

-- Examples to try, each with a key, a value as typed and how to read it.
local kExamples = {
    {id = 'name', key = 'profile.name', value = 'Ana', kind = 'text'},
    {id = 'coins', key = 'profile.stats.coins', value = '120', kind = 'number'},
    {id = 'tutorial', key = 'profile.tutorialDone', value = 'true', kind = 'boolean'},
    {id = 'recent', key = 'recent', value = 'slot-1, slot-3', kind = 'list'},
    {id = 'group', key = 'profile.stats', value = '', kind = 'text'},
    {id = 'through', key = 'profile.name.first', value = 'Ana', kind = 'text'},
    {id = 'empty', key = 'profile..name', value = 'Ana', kind = 'text'},
    {id = 'reserved', key = 'audio.volume.music', value = '0.5', kind = 'number'},
}

-- Turns the typed text into the value of its kind, or returns nil and why it cannot.
local function parse(text, kind)
    if kind == 'number' then
        local number = tonumber(text)
        return number, number == nil and text .. ' is not a number' or nil
    end
    if kind == 'boolean' then
        if text ~= 'true' and text ~= 'false' then
            return nil, 'a boolean is true or false'
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

function Values:init(entry)
    Values.super.init(self, entry)
    self.key = kExamples[1].key
    self.value = kExamples[1].value
    self.kind = kExamples[1].kind
end

function Values:content()
    local examples = {}
    for index, example in ipairs(kExamples) do
        examples[index] = {id = example.id, text = example.key, caption = example.value ~= '' and example.kind .. ' ' .. example.value or 'a group'}
    end
    local actions = {
        {id = 'set', text = 'Set', run = self.set},
        {id = 'get', text = 'Get', run = self.get},
        {id = 'has', text = 'Has', run = self.has},
        {id = 'remove', text = 'Remove', run = self.remove},
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
    return {
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
            ui.row{gap = 12, ui.sectionTitle{text = 'preferences.values()'}, ui.badge{id = 'dirty', text = 'Saved', tone = 'success'}},
            ui.scroll{grow = 1, ui.label{id = 'memory', text = '', font = 'monospace'}},
        },
        ui.panel{grow = 1, align = 'stretch', gap = 12,
            ui.sectionTitle{text = 'preferences.json'},
            ui.scroll{grow = 1, ui.label{id = 'disk', text = '', font = 'monospace'}},
        },
    }
end

function Values:enter()
    Values.super.enter(self)
    self:refresh()
end

function Values:pick(id)
    for _, example in ipairs(kExamples) do
        if example.id == id then
            self.key, self.value, self.kind = example.key, example.value, example.kind
            self:show('key', {value = example.key})
            self:show('value', {value = example.value})
            self:show('kind', {selected = example.kind})
        end
    end
end

-- Runs a call that may raise, such as a key with an empty part, and shows its result or its error.
function Values:report(call, ...)
    local ok, result = pcall(...)
    self:show('result', {text = ok and call .. ' ' .. result or call .. ' raised: ' .. tostring(result), color = ok and 'accentText' or 'dangerText'})
end

function Values:set()
    local value, problem = parse(self.value, self.kind)
    if kReserved[self.key:match('^[^.]*')] then
        problem = 'The group ' .. self.key:match('^[^.]*') .. ' belongs to the other tests of the sample, so this playground leaves it alone.'
    elseif self.value == '' then
        problem = 'Type a value first. A key that names a group, such as profile.stats, is read with Get.'
    end
    if problem then
        self:show('result', {text = problem, color = 'dangerText'})
        return
    end
    self:report(string.format("preferences.set('%s', value)", self.key), function()
        preferences.set(self.key, value)
        return 'stored it in memory'
    end)
end

function Values:get()
    self:report(string.format("preferences.get('%s') returned", self.key), function()
        return sample.json(preferences.get(self.key))
    end)
end

function Values:has()
    self:report(string.format("preferences.has('%s') returned", self.key), function()
        return tostring(preferences.has(self.key))
    end)
end

function Values:remove()
    self:report(string.format("preferences.remove('%s') returned", self.key), function()
        return tostring(preferences.remove(self.key))
    end)
end

function Values:save()
    self:report('preferences.save()', function()
        preferences.save()
        return 'wrote preferences.json'
    end)
end

function Values:reload()
    self:report('preferences.load()', function()
        preferences.load()
        return 'read preferences.json and dropped the unsaved changes'
    end)
end

-- Shows the preferences in memory next to the file on disk, with a badge that tells whether they differ.
function Values:refresh()
    local dirty = preferences.dirty()
    self:show('dirty', {text = dirty and 'Unsaved changes' or 'Saved', tone = dirty and 'warning' or 'success'})
    self:show('memory', {text = sample.json(preferences.values())})
    self:show('disk', {text = storage.exists('preferences.json') and storage.readText('preferences.json') or 'There is no preferences.json yet.'})
end

return Values
