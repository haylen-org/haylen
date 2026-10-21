-- Developer console: commands of the test with typed arguments that end with it, lines run from code with their results, completions and the history, and the console window that the grave accent key, a touch of four fingers or the button opens. The log shows each line run as a console does, the line in the accent color and its result under it.
local debugging = require('haylen.debug')
local haylen = require('haylen')
local ui = require('haylen.ui')

local Test = require('harness.test')
local LogCard = require('categories.development.log-card')

local Console = haylen.class('Console', Test)

Console.code = [[
debugging.addCommand('gold', function(amount)
    player.gold = player.gold + amount
    return 'Gold ' .. player.gold
end, {help = 'Adds gold to the player.', args = {'integer'}, owner = self})
local ok, output = debugging.runCommand('gold 50')
print(table.concat(debugging.completeCommand('go'), ', '))  debugging.setConsoleVisible(true)]]

function Console:enter()
    self.gold = 0
    self.god = false
    self.console = LogCard(self, 'log', 'Console', 'The lines run from code show here.', 'monospace')
    debugging.addCommand('gold', function(amount)
        self.gold = self.gold + amount
        self:set('state', {text = self:describe()})
        return 'Gold ' .. self.gold
    end, {help = 'Adds gold to the player.', args = {'integer'}, owner = self})
    debugging.addCommand('god', function(enabled)
        self.god = enabled
        self:set('state', {text = self:describe()})
        return enabled and 'Damage is off.' or 'Damage is on.'
    end, {help = 'Turns damage off or on.', args = {'boolean'}, owner = self})
    debugging.addCommand('say', function(text)
        return 'The player says: ' .. text
    end, {help = 'Prints a text.', args = {'text'}, owner = self})

    self:frame{
        hint = 'Open the console with the grave accent key, four fingers or the button, and type "gold 50", "god true" or "help".',
        code = Console.code,
        content = {ui.row{wrap = true, grow = 1, gap = 24, alignItems = 'stretch',
            ui.panel{width = 520, gap = 12,
                ui.label{text = 'The player', font = 'caption', color = 'textMuted'},
                ui.label{id = 'state', text = self:describe(), font = 'heading'},
                ui.button{id = 'open', text = 'Open the console', align = 'stretch', variant = 'primary', onClick = function() self:open() end},
                ui.button{id = 'run', text = 'Run "gold 10" from code', align = 'stretch', onClick = function() self:run('gold 10') end},
                ui.button{id = 'wrong', text = 'Run "gold ten" from code', align = 'stretch', onClick = function() self:run('gold ten') end},
            },
            self.console:node(),
        }},
    }
    self:run('gold 50')
    self:run('god true')
    self:run('say Hello there')
    local completions = {}
    for index, completion in ipairs(debugging.completeCommand('go')) do
        completions[index] = '"' .. completion:gsub('%s+$', '') .. '"'
    end
    self.console:add('Completions of "go"', table.concat(completions, ' and '))
end

function Console:exit()
    debugging.setConsoleVisible(false)
    Console.super.exit(self)
end

function Console:describe()
    return string.format('Gold %d, damage %s', self.gold, self.god and 'off' or 'on')
end

function Console:run(line)
    local ok, output = debugging.runCommand(line)
    self.console:add('> ' .. line, ok and (output ~= '' and output or 'Ran without output.') or ('Refused: ' .. output), not ok and 'danger' or nil)
end

function Console:open()
    if not debugging.consoleEnabled() then
        self.console:add('The console is off', 'The app runs outside development without "debug.console" in "app.json".', 'danger')
        return
    end
    debugging.setConsoleVisible(true)
end

return Console
