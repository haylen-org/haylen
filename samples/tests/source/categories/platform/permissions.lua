-- The state of every permission of the engine as this platform tells it, a request for each one with the answer of the person, and what a read of a clipboard that the person refused answers. The permissions of plugins, such as the camera and the motion sensors, belong to the plugins that own them. The states come again whenever the app comes back to the foreground, since the person may change them in the settings of the system meanwhile.
local haylen = require('haylen')
local system = require('haylen.system')
local ui = require('haylen.ui')

local Test = require('harness.test')

local Permissions = haylen.class('Permissions', Test)

Permissions.kinds = {{'screens', 'Every screen'}, {'localNetwork', 'Local network'}}

-- The words and the tone of each state.
Permissions.states = {
    granted = {'Granted', 'success'}, prompt = {'Not asked yet', 'information'}, denied = {'Denied', 'danger'}, unknown = {'Asked at the first use', 'warning'}, unsupported = {'Unsupported', 'neutral'},
}

Permissions.kept = 12

function Permissions:enter()
    self.lines = {}
    self.logged = 0
    local rows = {}
    for index, kind in ipairs(Permissions.kinds) do
        rows[index] = ui.row{gap = 16, alignItems = 'center',
            ui.label{text = kind[2], width = 300},
            ui.statusIndicator{id = 'state-' .. kind[1], text = 'Checking', tone = 'neutral', grow = 1},
            ui.button{id = 'ask-' .. kind[1], text = 'Ask', onClick = function() self:ask(kind[1]) end},
        }
    end
    self:listen('appActive', function()
        self:checkAll()
    end)
    self:frame{
        hint = 'Ask for a permission and answer the prompt of the system. Refuse a read of the clipboard to see the error that says why.',
        focus = 'ask-screens',
        controls = {
            ui.button{id = 'check', text = 'Check every permission', variant = 'primary', onClick = function() self:checkAll() end},
            ui.button{id = 'paste', text = 'Read the clipboard', onClick = function() self:paste() end},
            ui.label{text = 'A platform that never asks answers "granted", one that asks at the first use answers "unknown", and one without the capability answers "unsupported" with the reason.', color = 'textMuted', font = 'caption'},
        },
        content = {ui.row{grow = 1, gap = 24, align = 'stretch',
            ui.panel{grow = 1, align = 'stretch', gap = 12,
                ui.sectionTitle{text = 'Permissions'},
                ui.scroll{grow = 1, height = 0, ui.column{gap = 10, children = rows}},
            },
            ui.panel{width = 600, align = 'stretch', gap = 12,
                ui.sectionTitle{text = 'Answers'},
                ui.scroll{grow = 1, height = 0, focusable = false, ui.list{id = 'answers', items = {}}},
            },
        }},
        code = "local state = system.permission('localNetwork'):await()\nif state == 'prompt' then\n  state = system.requestPermission('localNetwork'):await()\nend\nlocal text, message, failure = system.clipboard():await()\nprint(failure and failure.code, message)",
    }
    self:checkAll()
end

function Permissions:record(text, detail)
    self.logged = self.logged + 1
    table.insert(self.lines, 1, {id = 'answer-' .. self.logged, text = text, caption = detail})
    self.lines[Permissions.kept + 1] = nil
    self:set('answers', {items = self.lines})
    self:log('%s: %s', text, detail)
end

-- Shows the state of a permission, or the reason of a platform without the capability with what the project lacks.
function Permissions:show(kind, state, message, failure)
    if not state then
        self:set('state-' .. kind, {text = failure.code == 'unsupported' and 'Unsupported' or failure.code, tone = 'neutral', tooltip = message})
        return message .. (failure.data and string.format(' The project lacks "%s".', failure.data.missing[1].name) or '')
    end
    local shown = Permissions.states[state]
    self:set('state-' .. kind, {text = shown[1], tone = shown[2], tooltip = ''})
    return string.format('The state is "%s".', state)
end

function Permissions:checkAll()
    self:spawn(function()
        local states = {}
        for _, kind in ipairs(Permissions.kinds) do
            local state, message, failure = system.permission(kind[1]):await()
            self:show(kind[1], state, message, failure)
            states[#states + 1] = kind[1] .. ' ' .. (state or failure.code)
        end
        self:log('Checked %s.', table.concat(states, ', '))
    end)
end

function Permissions:ask(kind)
    self:record(string.format('Asking for "%s"', kind), 'The prompt of the system shows, unless the person answered it before or the platform never asks.')
    self:spawn(function()
        local state, message, failure = system.requestPermission(kind):await()
        self:record(string.format('The request for "%s" answered', kind), self:show(kind, state, message, failure))
    end)
end

-- A clipboard that the person refused answers "denied" with the sentence of its own failure.
function Permissions:paste()
    self:spawn(function()
        local text, message, failure = system.clipboard():await()
        if failure then
            self:record(string.format('The clipboard failed with "%s"', failure.code), message)
            return
        end
        self:record('The clipboard was read', #text == 0 and 'It holds no text.' or string.format('It holds %d bytes of text.', #text))
    end)
end

return Permissions
