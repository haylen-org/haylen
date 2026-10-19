-- Conversation: a smith who greets once and then in variants, sells swords while the gold lasts, tells a rumor only once, hands commands to the test and saves the walk to resume it.
local assets = require('haylen.assets')
local haylen = require('haylen')
local ui = require('haylen.ui')

local Test = require('harness.test')

local Conversation = haylen.class('Conversation', Test)

Conversation.choiceCount = 3

function Conversation:enter()
    self.lines = {}
    self.talk = assets.load('algorithms/smith.conversation')
    local choices = {}
    for index = 1, Conversation.choiceCount do
        choices[index] = ui.button{id = 'choice' .. index, text = '', align = 'stretch', onClick = function() self:choose(index) end}
    end
    self:frame{
        hint = 'Talk to the smith: continue, buy swords while the gold lasts, ask once for news, or save and resume the conversation.',
        content = {ui.row{wrap = true, grow = 1, gap = 24,
            ui.panel{width = 640, align = 'stretch', gap = 12,
                ui.label{id = 'speaker', text = '', font = 'heading'},
                ui.label{id = 'line', text = ''},
                ui.button{id = 'continue', text = 'Continue', align = 'stretch', variant = 'primary', onClick = function() self:advance() end},
                ui.column{gap = 8, align = 'stretch', table.unpack(choices)},
                ui.row{wrap = true, gap = 12,
                    ui.button{id = 'save', text = 'Save', onClick = function() self.saved = self.state:save() self:note('Saved the conversation in the node "' .. self.state.node .. '".') end},
                    ui.button{id = 'resume', text = 'Resume', onClick = function() self:resume() end},
                    ui.button{id = 'restart', text = 'Start again', onClick = function() self:begin() end},
                },
            },
            ui.scroll{grow = 1, ui.label{id = 'log', text = '', grow = 1}},
        }},
    }
    self:begin()
    self:advance()
    self:choose(1)
end

function Conversation:begin()
    self.state = self.talk:start('greeting', {seed = 7})
    self.state:on('command', function(name, arguments)
        self:note(string.format('The command "%s" ran with %s.', name, arguments.item or arguments.hand or 'nothing'))
    end, {owner = self})
    self:show()
end

function Conversation:resume()
    if not self.saved then
        self:note('Nothing is saved yet.')
        return
    end
    self.state = self.talk:restore(self.saved)
    self:note('Resumed in the node "' .. self.state.node .. '".')
    self:show()
end

function Conversation:advance()
    if #self.state.choices > 0 or self.state.finished then
        return
    end
    self.state:advance()
    self:show()
end

function Conversation:choose(index)
    if index > #self.state.choices then
        return
    end
    self:note('Chose "' .. self.state.choices[index].text .. '".')
    self.state:choose(index)
    self:show()
end

function Conversation:show()
    local line = self.state.line
    local variables = self.state.variables
    self:set('speaker', {text = self.state.finished and 'The conversation ended' or line and line.speaker or ''})
    self:set('line', {text = line and line.text or string.format('Gold %d, swords %d.', variables.gold, variables.swords)})
    local choices = self.state.choices
    self:set('continue', {visible = #choices == 0 and not self.state.finished})
    for index = 1, Conversation.choiceCount do
        self:set('choice' .. index, {visible = choices[index] ~= nil, text = choices[index] and choices[index].text or ''})
    end
end

function Conversation:note(line)
    table.insert(self.lines, 1, line)
    self.lines[13] = nil
    self:set('log', {text = table.concat(self.lines, '\n')})
end

return Conversation
